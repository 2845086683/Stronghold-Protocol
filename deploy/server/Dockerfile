ARG PYTHON_IMAGE
FROM ${PYTHON_IMAGE}
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
WORKDIR /opt/stronghold
COPY src/StrongholdServer/stronghold_server /opt/stronghold/src/StrongholdServer/stronghold_server
COPY src/StrongholdServer/third_party/stronghold-protocol-reference /opt/stronghold/src/StrongholdServer/third_party/stronghold-protocol-reference
COPY server-data /opt/stronghold/server-data
COPY deploy/server /opt/stronghold/deploy/server
RUN mkdir -p /data && chown 10001:10001 /data && chmod 700 /data
USER 10001:10001
EXPOSE 16680
ENTRYPOINT ["python", "-I", "/opt/stronghold/deploy/server/manage.py"]
CMD ["serve", "--state-dir", "/data", "--host", "0.0.0.0", "--port", "16680", "--public-only"]
