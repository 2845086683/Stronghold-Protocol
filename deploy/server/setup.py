"""Explicit Docker setup: fixed official tags, locked digests, then healthy start.

No dependency installation, firewall edits, latest tags, or background downloads.
Call only from an operator's explicit `server.sh setup` action.
"""
from __future__ import annotations

import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from manage import OWNER_MARKER, atomic_json, checked_path, configure, private_dir, read_json, verify_owner


HERE = Path(__file__).resolve().parent
DIGEST = re.compile(r"sha256:[0-9a-f]{64}\Z")
TAG = {"python": re.compile(r"python:(3\.\d+\.\d+(?:-[A-Za-z0-9_.-]+)?)\Z"),
       "caddy": re.compile(r"caddy:(\d+\.\d+\.\d+(?:-[A-Za-z0-9_.-]+)?)\Z")}
REPOSITORIES = {name: {name, "library/" + name, "docker.io/library/" + name,
                       "index.docker.io/library/" + name, "registry-1.docker.io/library/" + name}
                for name in TAG}
PERSISTENT_VOLUMES = {"stronghold-private-server-data", "stronghold-private-caddy-data", "stronghold-private-caddy-config"}


def _prompt(input_fn, question):
    try:
        return input_fn(question).strip()
    except EOFError:
        raise RuntimeError("Interactive input unavailable; use --domain or --lan-address with --non-interactive") from None


def _new_destination(raw):
    path = checked_path(raw)
    if path.exists():
        entries = list(path.iterdir())
        if any(item.name == OWNER_MARKER for item in entries):
            verify_owner(path)
            entries = [item for item in entries if item.name != OWNER_MARKER]
        if entries:
            raise RuntimeError("setup requires a new/empty owned instance directory; use up or upgrade for an existing deployment")
    return path


def _call(runner, arguments, *, capture=False):
    try:
        return runner(arguments, capture=capture)
    except (OSError, subprocess.SubprocessError) as error:
        detail = getattr(error, "stderr", None)
        message = str(detail).strip()[-1200:] if detail else str(error)
        raise RuntimeError("Explicit Docker setup command failed: " + message) from None


def _object(text, description):
    try:
        value = json.loads(text)
    except (TypeError, ValueError):
        raise RuntimeError(description + " returned invalid JSON") from None
    if not isinstance(value, dict):
        raise RuntimeError(description + " did not return an object")
    return value


def _capabilities(runner):
    version = _call(runner, ["docker", "compose", "version", "--short"], capture=True)
    match = re.fullmatch(r"v?(\d+)\.(\d+)\.(\d+)(?:[-+][A-Za-z0-9_.-]+)?", version.strip())
    if not match or tuple(int(part) for part in match.groups()) < (2, 20, 0):
        raise RuntimeError("Docker Compose 2.20+ with --wait support is required; no software was installed")
    info = _object(_call(runner, ["docker", "info", "--format", "{{json .}}"], capture=True), "docker info")
    architecture = {"x86_64": "amd64", "aarch64": "arm64", "amd64": "amd64", "arm64": "arm64"}.get(info.get("Architecture"))
    server_version = info.get("ServerVersion")
    if info.get("OSType") != "linux" or not architecture or not isinstance(server_version, str) or not server_version:
        raise RuntimeError("A reachable Docker engine running Linux amd64/arm64 containers is required")
    return {"composeVersion": version.strip(), "serverVersion": server_version, "platform": "linux/" + architecture}


def _lock_image(runner, name, candidate, platform):
    match = TAG[name].fullmatch(candidate) if isinstance(candidate, str) else None
    if not match:
        raise RuntimeError("Runtime requirements must pin an exact official version tag for " + name)
    tag = match.group(1)
    repository = "docker.io/library/" + name
    canonical_tag = repository + ":" + tag
    _call(runner, ["docker", "pull", "--platform", platform, canonical_tag])
    image = _object(_call(runner, ["docker", "image", "inspect", "--format", "{{json .}}", canonical_tag], capture=True),
                    "docker image inspect")
    repo_tags = image.get("RepoTags")
    accepted_tags = {repo + ":" + tag for repo in REPOSITORIES[name]}
    if not isinstance(repo_tags, list) or not any(value in accepted_tags for value in repo_tags if isinstance(value, str)):
        raise RuntimeError("Inspected image does not carry the exact requested official version tag")
    image_id = image.get("Id")
    if not isinstance(image_id, str) or not DIGEST.fullmatch(image_id):
        raise RuntimeError("Inspected image has no valid immutable image ID")
    if image.get("Os") != "linux" or "linux/" + str(image.get("Architecture")) != platform:
        raise RuntimeError("Inspected image platform differs from the selected Docker engine")
    raw = image.get("RepoDigests")
    if not isinstance(raw, list) or not raw:
        raise RuntimeError("Official image RepoDigests is empty; no configuration was published")
    hashes = set()
    for value in raw:
        if not isinstance(value, str) or value.count("@") != 1:
            raise RuntimeError("Invalid image RepoDigest entry")
        repo, digest = value.split("@")
        if not DIGEST.fullmatch(digest):
            raise RuntimeError("Invalid image RepoDigest hash")
        if repo in REPOSITORIES[name]:
            hashes.add(digest)
    if len(hashes) != 1:
        raise RuntimeError("Expected exactly one digest in the official library/" + name + " namespace")
    digest = next(iter(hashes))
    return {"requestedTag": candidate, "pulledTag": canonical_tag, "repository": repository,
            "digest": digest, "lockedImage": candidate + "@" + digest, "imageId": image_id,
            "platform": platform, "source": "docker image inspect RepoDigests after explicit docker pull"}


def _empty_project(runner, command):
    raw = _call(runner, command + ["ps", "--all", "--format", "json"], capture=True).strip()
    if raw:
        try:
            value = json.loads(raw)
        except ValueError:
            # Some Compose versions emit one JSON object per container.
            try:
                value = [json.loads(line) for line in raw.splitlines() if line.strip()]
            except ValueError:
                raise RuntimeError("Cannot verify that this Compose project is new") from None
        if value != []:
            raise RuntimeError("An existing Compose project was found; setup will not replace or stop it. Use upgrade.")
    # `compose down` preserves data/certificates even after all containers are
    # gone. Such a deployment still needs its matching config and upgrade path.
    volumes = _call(runner, ["docker", "volume", "ls", "--format", "{{json .Name}}"], capture=True)
    try:
        names = [json.loads(line) for line in volumes.splitlines() if line.strip()]
    except (ValueError, AttributeError):
        raise RuntimeError("Cannot verify persistent Docker volume ownership") from None
    if any(not isinstance(name, str) for name in names):
        raise RuntimeError("Invalid Docker volume inventory")
    if PERSISTENT_VOLUMES.intersection(names):
        raise RuntimeError("Existing Stronghold data/certificate volumes were found; use the matching instance config and upgrade/restore. No volume was changed.")


def _publish(target, staging, provenance):
    # deployment.json is published last, so failure never leaves a usable new
    # configuration. Only files this transaction created may be removed.
    _new_destination(target)
    folder = private_dir(target)
    written = []
    try:
        atomic_json(folder / "image-lock.json", provenance); written.append(folder / "image-lock.json")
        raw = (staging / "compose.env").read_bytes()
        temporary = folder / "compose.env.setup-staging"
        descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(raw); stream.flush(); os.fsync(stream.fileno())
        os.replace(temporary, folder / "compose.env"); written.append(folder / "compose.env")
        atomic_json(folder / "deployment.json", read_json(staging / "deployment.json"))
    except Exception:
        for path in reversed(written):
            path.unlink(missing_ok=True)
        (folder / "compose.env.setup-staging").unlink(missing_ok=True)
        raise


def setup_deployment(config_dir, domain=None, lan_address=None, *, runner, command_builder,
                     non_interactive=False, start=True, input_fn=input, output_fn=print,
                     requirements_path=None):
    """Explicit first deployment; runner/command_builder reuse compose.py APIs.

    `runner(argv, capture=False)` returns captured stdout when capture=True.
    Tests inject a fake Docker CLI; production passes compose.run/command.
    """
    target = _new_destination(config_dir)
    if domain and lan_address:
        raise RuntimeError("Choose exactly one public domain or explicit private LAN address")
    if not domain and not lan_address:
        if non_interactive:
            raise RuntimeError("--non-interactive setup requires --domain or --lan-address")
        mode = _prompt(input_fn, "Mode [1=public HTTPS, 2=private LAN HTTP]: ")
        if mode in ("1", "public"):
            domain = _prompt(input_fn, "Public DNS domain (without scheme/path): ")
        elif mode in ("2", "lan"):
            lan_address = _prompt(input_fn, "This host's private LAN IPv4 address: ")
        else:
            raise RuntimeError("Select public HTTPS or private LAN HTTP explicitly")
    requirements_file = Path(requirements_path) if requirements_path else HERE / "runtime-requirements.json"
    requirements_raw = requirements_file.read_bytes()
    requirements = json.loads(requirements_raw.decode("utf-8"))
    if not isinstance(requirements, dict):
        raise RuntimeError("Runtime requirements must be an object")
    candidates = requirements.get("dockerCandidates", {})
    if not isinstance(candidates, dict) or requirements.get("format") != 1 or candidates.get("requireDigestAtConfiguration") is not True:
        raise RuntimeError("Reviewed fixed-tag runtime requirements are missing")
    for name in TAG:
        candidate = candidates.get(name)
        if not isinstance(candidate, str) or not TAG[name].fullmatch(candidate):
            raise RuntimeError("Only reviewed exact official version tags are accepted; latest/mirrors are not used")
    output_fn("Setup explicitly pulls the fixed official image tags and records their immutable digests; it does not install Docker or change firewall rules.")
    if domain:
        output_fn("Public HTTPS requires DNS A/AAAA pointing to this host and reachable TCP 80/443. The Compose backend health wait does not prove public DNS/TLS; verify the public HTTPS /health endpoint after setup.")
    else:
        output_fn("LAN mode uses HTTP on the explicit private IPv4 address:16680. It does not configure public NAT or firewall access.")
    with tempfile.TemporaryDirectory(prefix="stronghold-setup-") as temporary:
        stage = Path(temporary) / "instance"
        # Validate address and candidate syntax before any pull or target write.
        configure(stage, domain, lan_address, candidates["python"] + "@sha256:" + "0" * 64,
                  candidates["caddy"] + "@sha256:" + "0" * 64)
        capabilities = _capabilities(runner)
        locks = {name: _lock_image(runner, name, candidates[name], capabilities["platform"]) for name in TAG}
        config = configure(stage, domain, lan_address, locks["python"]["lockedImage"], locks["caddy"]["lockedImage"])
        provenance = {"schemaVersion": 1, "product": "StrongholdProtocol.DeploymentImageLock",
                      "createdUtc": dt.datetime.now(dt.timezone.utc).isoformat(), "mode": config["mode"],
                      "requirementsSha256": hashlib.sha256(requirements_raw).hexdigest(),
                      "requirementsSource": "deploy/server/runtime-requirements.json", "docker": capabilities,
                      "images": locks, "nativeClientValidated": False, "automaticLatest": False}
        command = command_builder(stage)
        started = False
        if start:
            _empty_project(runner, command)
            try:
                _call(runner, command + ["up", "-d", "--build", "--wait", "--wait-timeout", "90"])
                started = True
                _publish(target, stage, provenance)
            except Exception:
                # The project was verified empty immediately before up. Stop
                # only this new project, preserving named volumes and evidence.
                try:
                    _call(runner, command + ["stop"])
                except RuntimeError:
                    output_fn("Setup failed and graceful stop also failed; inspect this Compose project before retrying. No valid instance configuration was published.")
                raise
        else:
            _publish(target, stage, provenance)
        return {"configured": True, "started": started, "composeHealthWaitPassed": started,
                "publicHttpsValidated": False, "mode": config["mode"],
                "endpoint": "https://" + config["site"] if domain else "http://" + str(lan_address) + ":16680",
                "imageLock": "image-lock.json", "images": {name: locks[name]["lockedImage"] for name in locks},
                "nativeClientValidated": False}
