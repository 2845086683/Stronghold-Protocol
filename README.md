# 明日方舟《卫戍协议：盟约（下半）》活动原生模拟器

本项目面向《明日方舟》国服 Windows 客户端，通过客户端插件与独立 Python 服务端，提供「卫戍协议：盟约（下半）」活动的私有运行环境。客户端沿用原生 Unity 界面、战斗引擎与已有游戏资源；服务端负责活动资料、队伍配置、商店经济、棋盘状态、效果结算、阶段推进和持久化。

项目作者B站主页：[繁花掠影](https://space.bilibili.com/13552115)

相关视频教程[点击此处查看](https://www.bilibili.com/video/BV1gtnD6UEBX/)

> **非官方声明**：本项目是非官方玩家项目，与上海鹰角网络科技有限公司（Hypergryph）、Yostar 及其关联方没有隶属、授权或背书关系。项目定位为个人学习、研究与单机／可信局域网非商业娱乐；相关许可与游戏资源权属见本文末尾的许可声明。游戏资源不属于本项目代码的 GPL 授权范围。

## 1. 项目架构

项目将活动逻辑与原生战斗执行分开：服务端维护活动状态，插件把私有服务响应转换为原生客户端可以使用的数据与回调，Unity 执行实际战斗。

| 组成 | 职责 | 代码位置 |
| --- | --- | --- |
| 客户端插件 | 活动入口、资料投影、原生会话、准备界面、棋盘、战斗桥接、多人回调与状态恢复 | [`src/plugin/`](src/plugin/) |
| 编译库代理 | 转发原始 `d3dcompiler_47.dll` 导出，并按配置加载独立插件 | [`src/proxy/`](src/proxy/) |
| 活动服务端 | 活动领域规则、HTTP 接口、多人房间、协议数据投影、SQLite 持久化 | [`src/server/`](src/server/) |
| 客户端部署器 | 包与客户端校验、持久备份、安装、升级、配置、回滚与卸载 | [`deploy/client/`](deploy/client/) |
| 服务端部署器 | Windows 前台／后台开服、局域网地址展示、日志跟踪、Linux Compose、备份与恢复 | [`deploy/server/`](deploy/server/) |

服务端与游戏进程独立：启动游戏不会自动启动服务端，退出游戏也不会停止服务端。本项目不提供独立游戏客户端、官方登录绕过或完整官方服务器替代；活动桥接协议也不承诺与官方封包兼容。

## 2. 功能与实现范围

以下描述的是仓库中已有的实现，不能用作所有地图、模式及多人场景的实机通过证明。

| 领域 | 已有实现 |
| --- | --- |
| 活动资料与准备 | 活动资料、干员与棋子配置、技能／装备选择、队伍快照、配置写入和会话恢复 |
| 对局与经济 | 商店刷新、购买与合成、有限棋池、棋盘部署、地形、手牌、轮次与阶段推进 |
| 效果与奖励 | 羁绊、驻守、道具、经济效果、特殊选择、晋升奖励、结算称号与多人奖杯 |
| 原生战斗桥接 | 战斗准备、资源预检、启动与结束回报、结算投影、会话退出及待确认请求记录 |
| 多人房间 | 参与者注册与认证、六位房间码、最多四席、准备确认、阶段时钟、聊天与表情 |
| 多人协作 | 场地与棋盘投影、观战、协防、Boss 阶段、物品赠送、成员离开与重连相关逻辑 |
| 状态持久化 | SQLite WAL、命令回执、重复请求校验、房间快照及按参与者隔离的单人数据库 |
| 部署与维护 | 本机服务管理、客户端安装校验、资源所有权记录、在线备份、恢复校验、Docker 镜像版本管理 |

服务端使用 Python 标准库，部署控制器没有额外 pip 依赖；客户端插件为 Windows 原生 C++ 实现，使用 IL2CPP 接口及 MinHook 等组件。

## 3. 仓库与发布包

请从仓库的 Releases 页面获取最新发布包。更换发布包后，应以新包清单为准；插件版本号不代表游戏客户端版本号。

**普通使用请下载并完整解压发布包。源码目录本身不是可直接安装的完整发布包。** 下文 Windows 命令在解压后的发布包根目录执行，不是源码仓库根目录；Linux 命令在服务端 `server/` 根目录执行。详细操作另见[客户端教程](release/client/README.md)与[服务端教程](release/server/deploy/server/README.md)。

## 4. 环境要求与版本兼容

### 4.1 环境要求

| 场景 | 要求 |
| --- | --- |
| 游戏客户端 | 与包清单精确匹配的《明日方舟》国服 Windows 客户端 |
| Windows 本机／局域网使用 | Windows 10/11 x64、系统自带 Windows PowerShell 5.1、游戏目录与解压目录写权限 |
| Windows 运行时 | ZIP 已含 CPython，无需另装 Python、Node.js、PowerShell 7 或编译器 |
| 自选 Python | 服务端控制器要求 CPython 3.12+，且具备 `sqlite3`、`ssl`；包内固定运行时配置见 [`runtime-requirements.json`](deploy/server/runtime-requirements.json) |
| Linux 局域网主机 | Docker Engine、Compose v2、运行部署控制器的系统 Python 3、明确的本机私有 IPv4 地址 |
| 首次 Linux 配置 | 能拉取运行时配置指定的 Python／Caddy 官方镜像；向导自动完成镜像配置 |

Windows 默认启动入口直接支持局域网：监听 `0.0.0.0:16680`，自动列出本机和网卡访问地址，不需要 Docker 或 Linux。本机访问 `http://127.0.0.1:16680`，其他设备使用终端列出的实际局域网 IP。原有回环单人模式保留在 `Start-Local-Server.cmd`。具体硬件需求仍取决于官方客户端及所运行场景，本仓库未提供额外性能基准。

### 4.2 版本兼容说明

使用与发布包匹配的国服 Windows 客户端。安装器和运行时会自动进行必要检查，用户无需手工计算或比对文件校验值；本文不再列出此类操作流程，程序中的自动检查保持启用。

声明 `native-index-cab-and-logical-name` 资源兼容策略的新包支持同一客户端程序构建下的资源热更新；程序文件或 metadata 版本变化仍需要配套适配。资源更新后是否可用还需启动游戏确认，不能仅凭“游戏已更新到最新版”判断兼容。安装器提示版本或文件不匹配时，应使用配套发布包，避免混用不同版本的 DLL 与资源。

## 5. Windows 安装与开服

### 5.1 准备目录

1. 准备与清单匹配的国服 Windows 客户端。
2. 完全退出游戏、启动器及 `PlatformProcess.exe`、`CefViewWing.exe` 等相关辅助进程。
3. 完整解压发布包，例如放在 `D:\Stronghold-Protocol-Local`。使用可写、层级较浅的本地目录，避免 `Program Files`、只读位置或直接从压缩包窗口运行。
4. 保留包内 `client/` 与 `server/` 层级，不要只复制几个 `.cmd` 文件。

### 5.2 选择启动模式并检查服务端

| 使用方式 | 发布包根目录入口 | 行为 |
| --- | --- | --- |
| 默认开服，本机或局域网玩家使用 | `Start-Server.cmd` | LAN 前台模式，监听 `0.0.0.0:16680`，显示地址与日志 |
| 保留原有本机单人资料入口 | `Start-Local-Server.cmd` | Local 前台模式，仅监听 `127.0.0.1:16680` |
| 关闭启动窗口后仍继续运行 | `Start-Server-Background.cmd` | LAN 后台模式，通过日志查看器观察运行情况 |
| 查看运行日志 | `Show-Server-Logs.cmd` | 持续显示当前实例日志，退出查看器不会停服 |
| 查看／停止服务 | `Status-Server.cmd`／`Stop-Server.cmd` | 查询当前实例，或请求其正常退出 |

默认双击 `Start-Server.cmd`，或在 PowerShell 中执行：

```powershell
.\Start-Server.cmd
```

保持这个前台窗口运行，在**另一个 PowerShell 窗口**检查服务：

```powershell
Invoke-RestMethod 'http://127.0.0.1:16680/health'
```

首次启动会创建 `server\local-server\deployment\`，生成数据库、进程记录、日志与服务端管理令牌。前台启动成功后会显示类似信息，实际网卡名称与地址以本机为准：

```text
ready pid=12345 address=0.0.0.0:16680 publicOnly=True
Connect on this PC: http://127.0.0.1:16680
Health check: http://127.0.0.1:16680/health
LAN client address [Ethernet, default route]: http://192.168.1.20:16680
```

健康响应中的 `ok: true` 表示服务可达，`publicMultiplayerOnly: true` 表示默认 LAN 模式采用参与者认证；`Start-Local-Server.cmd` 对应的值为 `false`。字段名中的 `public` 不代表自动向互联网开放。`nativeReady: false`、`officialWireCompatible: false` 也不代表启动失败，它们分别说明原生实机验收和官方封包兼容的声明边界。

默认入口是**前台服务**：保持窗口开启，按 `Ctrl+C` 正常停服，也可从另一窗口运行 `Stop-Server.cmd`。只有 `Start-Server-Background.cmd` 启动的后台实例不依赖原启动窗口。不同模式共用默认状态目录，切换模式前先停止原实例。

局域网玩家连接步骤和防火墙设置见[局域网部署](#7-局域网部署)。独自使用默认 LAN 模式时，本机也通过参与者身份保存资料；需要原有共享单人资料入口时，改用 `Start-Local-Server.cmd` 并重新配置客户端。

### 5.3 安装插件

确认游戏仍处于退出状态，然后双击 `Install-Client.cmd`，或执行：

```powershell
.\Install-Client.cmd
```

此入口默认配置 `http://127.0.0.1:16680`。**先开服，再安装或重新配置本机客户端**：安装器检测到 LAN 服务后，会自动启用参与者模式。安装器优先使用保存的设置，并尝试发现游戏目录；无法唯一定位时，需要输入**包含 `Arknights.exe` 的游戏根目录**。

安装器会验证客户端版本和发布包，备份已有文件，安装代理、插件与私有资源，并写入配置及所有权记录。首次安装与后续维护备份默认保存在：

```text
%LOCALAPPDATA%\StrongholdProtocol\ClientDeployment\backups\
```

需要明确指定路径或自动化安装时，使用发布包内的 PowerShell 入口：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\client\Deploy-Client.ps1 -Action Install -GameRoot 'D:\Games\Arknights Game' -Endpoint 'http://127.0.0.1:16680' -NonInteractive
```

### 5.4 进入游戏与运行确认

1. 安装成功后，访问 `/health` 确认服务端运行正常。
2. 正常通过官方启动器启动游戏，进入对应活动入口。
3. 依次检查资料、队伍准备、棋盘、战斗与结算；遇到异常按[常见问题](#9-常见问题)保留诊断信息。

安装器不会代为启动游戏或服务端，也不会替代官方客户端所需的正常启动／登录流程。

## 6. 客户端配置与维护

### 6.1 常用入口

以下命令在发布包根目录执行；安装、配置与卸载前均应退出游戏及相关辅助进程。

| 操作 | 双击入口 | 明确指定参数的 PowerShell 示例 |
| --- | --- | --- |
| 安装／升级 | `Install-Client.cmd` | `& .\client\Install.cmd -GameRoot 'D:\Games\Arknights Game' -Endpoint 'http://127.0.0.1:16680'` |
| 修改地址 | `Configure-Client.cmd` | `& .\client\Configure.cmd -Endpoint 'http://192.168.1.20:16680'` |
| 校验安装 | `Verify-Client.cmd` | `& .\client\Verify.cmd -GameRoot 'D:\Games\Arknights Game'` |
| 卸载还原 | `Uninstall-Client.cmd` | `& .\client\Uninstall.cmd -GameRoot 'D:\Games\Arknights Game'` |

根目录 `Install-Client.cmd` 已预设本机地址。安装到其他服务器时，直接使用 `client\Install.cmd -Endpoint ...`，避免在根目录包装器后重复传入同名参数。

### 6.2 地址与身份配置

实际生效的配置位于**游戏目录**：

```text
<游戏目录>\plugin\StrongholdProtocolCN\cn-activity.ini
```

默认 LAN 服务已启动时，本机安装后配置类似：

```ini
[listener]
mode=listen

[backend]
endpoint=http://127.0.0.1:16680
participant_profile=1
access_token=
```

| 配置项 | 说明 |
| --- | --- |
| `endpoint` | HTTP／HTTPS origin，可含端口；不接受额外路径、账号密码、查询参数、片段或 IPv6。HTTPS 证书需要被 Windows 正常信任 |
| `participant_profile` | 本机 `localhost`／`127.*` 默认 `0`；其他地址默认 `1`。若健康检查明确返回 `publicMultiplayerOnly=true`，即使连接回环地址也使用 `1` |
| `access_token` | 安装器写为空值；玩家不需要服务端 `operator.token` |
| `mode` | 安装器配置为 `listen`，用于启用插件活动桥接 |

建议通过配置入口修改地址，修改后重启游戏才会生效。地址不可达时安装器仍可按地址规则生成配置，**配置成功不代表连接测试成功**。本机连接回环地址且服务离线时，会按旧本机模式生成配置；若随后启动 LAN 服务，应保持服务在线，再运行 `Configure-Client.cmd -Endpoint http://127.0.0.1:16680`，使配置更新为参与者模式。切换至 Local 模式时同样重新配置。

启用参与者配置后，插件使用独立 participant 身份访问服务端，并按 endpoint 隔离本地凭据；凭据通过 Windows DPAPI 保护。它不是官方账号身份，也不应替换为服务端管理令牌。同一用户在更换服务器地址后，可能使用不同的参与者资料。本机共享单人资料与参与者独立资料也不会自动合并，切换模式或地址前应保留备份。

### 6.3 安装、升级与卸载行为

- **安装校验**：拒绝覆盖未知安装、已修改的受管文件或不匹配的客户端版本。
- **资源隔离**：活动 overlay 安装在 `<游戏目录>\plugin\StrongholdProtocolCN\resources\client-data\`；通过 `deployment-resources.json` 记录所有权，不替换原游戏资源索引。
- **升级回滚**：核验旧安装及清单，保留备份；失败时尝试恢复已验证的旧版本。若回滚未完成，会输出错误并保留恢复材料。
- **卸载还原**：恢复安装记录对应的原始编译库，仅移除可验证归本插件所有的文件。未知文件或用户修改过的配置可能保留。
- **保留数据**：卸载客户端插件不会自动删除服务端进度、本机备份、诊断文件或参与者凭据。

`%LOCALAPPDATA%\StrongholdProtocol\ClientDeployment\settings.json` 保存游戏目录、地址与版本，`last-backup.json` 指向最近的维护备份。使用自定义 `-StateRoot` 时，后续操作需继续指定相同目录，且该目录必须位于解压目录之外。

## 7. 局域网部署

Windows 原生服务端和 Linux Docker 均支持局域网。Windows 使用发布包即可开服；Linux 适合已有 Docker 主机的部署环境。两者的玩家端都使用配套的 Windows 客户端插件。

### 7.1 Windows 局域网开服

1. 主机运行 `Start-Server.cmd`，等待 `ready` 和 `LAN client address` 地址出现。默认监听所有 IPv4 网卡，无需预先填写主机 IP。
2. 从列出的网卡地址中选择与玩家处于同一网络的地址，例如 `http://192.168.1.20:16680`。私有地址优先、其中带默认路由的地址优先显示；VPN、Hyper-V 等网卡也可能列出，不能只凭显示顺序认定可达。
3. 另一台电脑先访问 `http://192.168.1.20:16680/health`，确认 `ok: true`、`publicMultiplayerOnly: true`。
4. 每台玩家电脑退出游戏，按本节“连接玩家客户端”配置相同的主机地址，然后重启游戏，建立或加入私有同盟。

`0.0.0.0` 是服务端监听地址，不能填写到客户端；其他玩家也不能用他们自己电脑上的 `127.0.0.1` 连接主机。服务端列出的地址不等于已完成跨电脑连通性测试；发现失败时可运行 `ipconfig` 查找主机 IPv4 地址。主机 IP 变化后应更新其他玩家的 endpoint，服务端继续监听 `0.0.0.0` 无需跟着改绑地址。

如只有本机可访问，在可信局域网中确认 Windows 当前网络为“专用”，检查无线 AP／访客网络隔离、防火墙与 VPN 路由。需要添加防火墙规则时，在**管理员 PowerShell** 中执行：

```powershell
New-NetFirewallRule -DisplayName 'Stronghold LAN 16680' -Direction Inbound -Action Allow -Protocol TCP -LocalPort 16680 -RemoteAddress LocalSubnet -Profile Private
```

该规则仅允许专用网络下本地子网访问 TCP 16680，没有固定某个主机 IPv4；使用自定义端口时同步调整规则。程序不会自动创建规则、改变网络分类或配置路由器，也不需要为了联机关闭整个防火墙。

要只监听指定网卡、使用自定义端口或转为后台运行，可按[服务端管理](#81-windows-管理命令)操作。

### 7.2 Linux Docker 局域网部署

#### 7.2.1 准备主机与服务端目录

主机需要 Docker Engine、Compose v2 和系统 Python 3；运行控制器的用户需要有调用 Docker 的权限。将完整发布包中的 `server/` 复制到主机，例如 `/opt/stronghold-local/server`，保留源码、数据及部署目录的相对层级。

如需在受支持的 Ubuntu 上使用随包依赖安装辅助脚本，可先审阅脚本，再显式执行：

```sh
cd /opt/stronghold-local/server
sudo sh deploy/server/Install-Docker-Ubuntu.sh --install
```

该辅助脚本会添加 Docker 软件源、安装依赖并启动 Docker 服务；它不配置防火墙，也不自动将用户加入 Docker 用户组。已有 Docker 环境无需执行。正式部署前确认：

```sh
python3 --version
docker version
docker compose version
```

#### 7.2.2 首次配置并启动

先确认主机实际持有的私有 IPv4 地址，例如 `192.168.1.20`，再运行：

```sh
cd /opt/stronghold-local/server
sh deploy/server/server.sh setup --lan-address 192.168.1.20 --non-interactive
```

也可运行 `sh deploy/server/server.sh setup`，在向导中选择私有 LAN HTTP 模式。

向导按 `runtime-requirements.json` 拉取并记录固定官方镜像版本，构建后端并等待健康检查，然后发布 `deploy/server/instance/` 下的配置与 `image-lock.json`。默认的 Compose 项目与数据卷名称固定，首次 `setup` 会拒绝接管已有同名项目或卷；已有实例应使用 `up`、`upgrade` 或 `restore`。

可使用 `--no-start` 只生成配置；这种情况下还没有启动服务。已配置的实例可执行：

```sh
sh deploy/server/server.sh verify
sh deploy/server/server.sh up
curl http://192.168.1.20:16680/health
```

局域网健康检查应返回 `ok: true` 和 `publicMultiplayerOnly: true`。这里的 `publicMultiplayerOnly` 表示采用参与者认证的受限桥接模式，**不表示已经暴露到公网**。网关仅转发 `/health` 与 `/bridge/act2autochess/multiplayer/*`，旧的共享单人／调试路由不会向局域网开放。代码还包含该认证前缀下的参与者独立单人桥接，不能将它与旧共享单人接口混用。

Linux Compose 的 `--lan-address` 仅接受 RFC1918 私有 IPv4 或回环地址；需要其他电脑访问时应使用主机的实际私有地址，不能填写 `127.0.0.1`、`0.0.0.0` 或公网 IP。

### 7.3 连接玩家客户端

每台玩家电脑退出游戏后，在 Windows 发布包根目录执行：

```powershell
& .\client\Install.cmd -Endpoint 'http://192.168.1.20:16680'
```

已经安装的客户端执行：

```powershell
& .\client\Configure.cmd -Endpoint 'http://192.168.1.20:16680'
```

以上配置步骤同时适用于 Windows 与 Linux 主机。endpoint 只填服务器 origin，不带 `/health`。Windows 主机上的本机玩家也可连接 `http://127.0.0.1:16680`，但应先开服再配置。各客户端应使用配套版本，并能直接访问该地址的 `/health`。插件会使用各自的参与者身份；无需向玩家分发服务器管理令牌。

### 7.4 网络与高级部署边界

- 主机应仅向可信局域网设备开放 TCP `16680`；HTTP 模式不适合在不可信网络中传送参与者凭据。
- 检查主机防火墙、无线网络设备隔离及容器端口发布规则；部署工具不会自动配置防火墙、NAT 或路由。
- Docker 的端口发布规则可能与主机防火墙预期不同，应核对实际可访问范围。
- Linux Docker 后端位于 Compose 内部网络，由 Caddy 网关发布外部端口；Windows LAN 模式由 Python 直接监听，无需 Caddy。两者默认均为单一后端进程。

源码另含 `compose.public.yaml` 与 `setup --domain <域名>` 的 HTTPS 部署入口。它要求正确的域名解析、可达的 TCP 80/443 和可信证书；配置向导仍明确保留 `publicHttpsValidated: false`，需要部署者从实际网络另外验证。**该入口的存在不构成公网运行或原生多人流程的验收承诺**；本文主要使用指南以本机与可信局域网为范围。高级部署细节可查看包内 `server/deploy/server/README.md`。

## 8. 服务端运维与数据恢复

### 8.1 Windows 管理命令

在发布包根目录执行。下列是可选择的管理操作，不应在已有实例运行时再次启动同一实例：

```powershell
# 前台 LAN 服务：保持窗口开启，Ctrl+C 正常停服
.\server\deploy\server\Server.ps1 Run -Mode LAN

# 或者后台启动 LAN 服务
.\server\deploy\server\Server.ps1 Start -Mode LAN
```

另开窗口管理当前实例：

```powershell
.\server\deploy\server\Server.ps1 Status
.\server\deploy\server\Server.ps1 Logs
.\server\deploy\server\Server.ps1 Backup
.\server\deploy\server\Server.ps1 Stop
```

`Logs` 持续跟踪日志，按 `Ctrl+C` 只退出查看器；要继续执行后续命令，可先退出查看器或另开窗口。`Status` 返回是否运行、监听地址、端口、模式及日志路径。直接调用 `Server.ps1` 而不传动作时，默认也是 `Run -Mode LAN`。

| 参数 | 用途 |
| --- | --- |
| `-Mode LAN` | 默认模式；使用参与者认证，默认监听 `0.0.0.0` |
| `-Mode Local` | 旧本机单人模式；默认监听 `127.0.0.1`，不对局域网开放 |
| `-BindAddress '192.168.1.20'` | 启动时只绑定指定 IPv4 网卡地址；非回环监听必须使用 LAN 模式 |
| `-Port 16681` | 修改监听端口；客户端 endpoint 和防火墙规则需同步修改 |
| `-StateDirectory 'D:\StrongholdState'` | 使用独立持久状态目录；后续启动、状态、日志、停止、备份与恢复均需指定同一路径 |
| `-PythonExe 'C:\Python313\python.exe'` | 指定满足要求的 CPython，替代包内运行时 |
| `-BackupDirectory 'D:\StrongholdBackups'` | 指定备份存放位置 |
| `-Source '<备份文件夹>'` | 为 `Restore` 指定恢复来源，不能只传数据库文件 |

例如，停止现有实例后，以新端口后台启动 LAN 服务：

```powershell
.\server\deploy\server\Server.ps1 Stop
.\server\deploy\server\Server.ps1 Start -Mode LAN -Port 16681
& .\client\Configure.cmd -Endpoint 'http://127.0.0.1:16681'
```

其他玩家需同步改用主机实际局域网 IP 的 16681 端口。切换模式、监听地址或端口前均应先停止当前实例；后台 `Start` 发现配置与正在运行的实例不同会明确拒绝。根目录管理入口默认使用标准状态目录，使用自定义目录时持续通过带参数的 `Server.ps1` 管理。启动脚本会转发附加参数，例如 `.\Start-Server.cmd -Port 16681`。

Python 底层管理器的独立调用默认仍是回环监听，不等同于 Windows 包装器的默认 LAN 模式；高级用户直接调用时可以显式传入 `serve --host 0.0.0.0 --public-only`，后台 `start` 也已支持传递这两个参数。

### 8.2 数据与日志位置

Windows 默认状态目录：

```text
server/local-server/deployment/
├─ .stronghold-deployment-owner.json  目录所有权标记
├─ stronghold.sqlite3                主数据库与活动／房间状态
├─ stronghold.sqlite3-wal             运行时可能存在的 WAL 文件
├─ stronghold.sqlite3-shm             运行时可能存在的共享内存文件
├─ participant-solo/                 按参与者隔离的单人存档
├─ secrets/operator.token            仅服务端持有的管理令牌
├─ process.json                      服务运行时的进程与实例记录
├─ logs/server.log                   服务内部滚动日志
├─ logs/bootstrap.log                后台启动输出与异常
└─ backups/                          自动命名的备份文件夹
```

客户端原生诊断文件位于：

```text
%LOCALAPPDATA%\StrongholdProtocol\captures\CN-native\cn-activity-<进程号>.json
```

终端和服务内部滚动日志记录启停、写请求结果与 HTTP 错误；成功的健康检查和 GET 轮询不刷屏。请求日志使用方法、状态与固定路由标签，不记录 `Authorization`、请求正文、参与者凭据、房间码或查询串。**这一约束不覆盖所有其他文件**：Caddy 网关有独立访问日志，安装备份可能保留旧配置；公开诊断资料前仍需检查并脱敏。

### 8.3 在线备份与恢复

双击 `Backup-Server.cmd` 或运行 `Server.ps1 Backup`。备份使用 SQLite backup API，包含已提交的 WAL 数据、主数据库与参与者单人数据库，并保存恢复所需的清单。

备份不包含 `operator.token`，但包含参与者身份及房间资料，应保存在受限目录，并保留备份目录内的清单与所有权标记。

恢复示例中的目录名需要替换为实际备份目录：

```powershell
.\server\deploy\server\Server.ps1 Stop
.\server\deploy\server\Server.ps1 Restore -Source 'D:\StrongholdBackups\20261024T120000Z-1234abcd'
.\server\deploy\server\Server.ps1 Start -Mode LAN
```

恢复入口自动检查备份及版本兼容性，通过后先备份当前数据，再替换数据库。运行中的实例由状态锁阻止恢复。遇到 `backup code/catalog differs`，应先使用匹配的旧发布包恢复，不要修改清单绕过检查。

恢复后按原先用途选择 `Start -Mode LAN` 或 `Start -Mode Local`，再配置客户端。不要直接复制正在写入的 `.sqlite3` 作为备份，也不要遗漏 `participant-solo/`。重置进度时，先停止服务，将整个状态目录改名保留，再启动生成新状态；这也会创建新的服务端身份环境，应提前保留所需资料。

### 8.4 Linux 管理与持久卷

在 Linux 的 `server/` 根目录执行：

```sh
sh deploy/server/server.sh status
sh deploy/server/server.sh logs
sh deploy/server/server.sh backup
sh deploy/server/server.sh stop
```

`logs` 持续跟踪最近日志；`backup` 在线生成备份，再复制到主机 `deploy/server/backups/`。恢复时执行：

```sh
sh deploy/server/server.sh restore --source /opt/stronghold-backups/20261024T120000Z-1234abcd
```

恢复入口负责停止服务、验证备份、执行恢复并重新启动。自定义 `--config-dir`、`--backup-dir` 时，应在后续管理中保持一致。

| 命名卷 | 内容 |
| --- | --- |
| `stronghold-private-server-data` | `/data` 下的数据库、参与者单人存档、令牌与服务内部日志 |
| `stronghold-private-caddy-data` | Caddy 持久数据及证书 |
| `stronghold-private-caddy-config` | Caddy 持久配置 |

备份命令不导出 Caddy 的两个持久卷；如需完整迁移，应单独保留相应资料。不要使用 `docker compose down -v` 或 `docker volume prune` 清理本项目数据，也不要将同一数据库卷同时交给多个运行中的后端实例。

### 8.5 升级与回退

1. 保留当前发布包、客户端备份、服务端备份与部署配置。
2. 将新版解压到新目录，核对客户端版本与发布包是否匹配。
3. Windows 使用原有 `-StateDirectory` 管理持久状态；Linux 保留并复制原 `deploy/server/instance/` 配置，继续使用原命名卷。
4. Linux 在新目录运行 `sh deploy/server/server.sh upgrade`；该入口先备份当前运行服务，再构建新后端并等待健康检查。
5. 退出游戏，使用新包客户端安装入口完成升级，再核对健康检查及实际活动流程。

数据库恢复校验会约束代码／数据版本。回退时应使用与升级前备份匹配的旧发布包执行恢复；不能只换回旧 DLL 或把旧源码接到可能已变化的新数据库上。镜像配置沿用已记录的固定版本，升级不会隐式切换到 `latest`。

## 9. 常见问题

| 现象 | 优先检查 | 处理方式 |
| --- | --- | --- |
| `No packaged Python runtime` | `server\runtime\windows\python.exe` 是否存在 | 优先重新完整解压；运行时目录缺失时可使用包内 `Setup-Runtime.cmd`，或通过 `-PythonExe` 指定兼容解释器 |
| 服务启动失败或超时 | 端口、`bootstrap.log`、`server.log`、状态目录 | 确认未用同一状态目录启动两个实例，先读错误再决定是否更换端口 |
| 安装器提示文件／版本不匹配 | 发布包是否完整、游戏是否更新、文件是否被修改 | 使用匹配包或修复游戏文件，避免混用不同版本 |
| 找不到游戏目录 | 是否选到了启动器目录或错误层级 | 传入包含 `Arknights.exe` 的绝对 `-GameRoot` |
| 配置成功但连接失败 | `/health`、实际游戏目录中的 `cn-activity.ini` | 核对地址和端口，确认服务在线，修改地址后重启游戏 |
| 活动入口缺失或空白 | 插件校验、资源文件、客户端版本、原生诊断 JSON | 运行 `Verify-Client.cmd`，检查包是否完整及文件是否被隔离；保留失败阶段信息 |
| 局域网无法连接 | 从玩家电脑访问主机 `/health` | 核对私有 IP、防火墙、无线网络隔离；玩家端不能填自己的 `127.0.0.1` |
| 旧共享接口返回 403／404 | 是否连接 LAN／受限部署 | 使用客户端参与者桥接；受限网关不开放旧共享单人／调试路径 |
| 前台窗口运行后无法输入其他命令 | 是否启动了 `Start-Server.cmd` | 服务正在前台运行，另开窗口配置／查询；需要后台模式时先停止再运行 `Start-Server-Background.cmd` |
| 地址列表出现多张网卡或没有 LAN 地址 | Wi-Fi／网线、VPN 与虚拟网卡 | 选择与玩家同网的实际 IPv4；自动发现失败时使用 `ipconfig`，不把 `0.0.0.0` 当作客户端地址 |
| 本机切换 LAN 后资料异常或接口报错 | 服务在线时是否重新配置过客户端 | 服务启动后重跑配置入口；LAN 参与者资料与旧 Local 共享单人资料不会自动合并 |
| 启动提示 bind／port／mode 不同 | 当前实例是否仍在运行 | 先停止同一状态目录的实例，再按新参数启动并更新客户端地址 |
| 恢复提示代码／数据不匹配 | 备份所对应的发布包 | 回到匹配发布包执行恢复，保留原清单与目录标记 |
| 路径过长或拒绝重解析路径 | 游戏、解压及备份根目录深度；是否有 junction／symlink | 使用较浅的真实本地目录；不要通过修改系统长路径设置来绕过安装器自身限制 |
| 存在 `deployment-resources.json.transaction.json` | 是否发生过中断安装或资源复制失败 | 保留该文件，按其中 `backupFolder` 核对恢复资料；不要直接删除事务记录后继续覆盖 |
| 卸载后仍有配置／诊断文件 | 文件是否被用户修改、是否属于运行数据 | 先确认原始编译库已还原，保留需要的备份；必要时通过官方启动器修复游戏 |

本机端口与日志可用以下命令检查：

```powershell
Get-NetTCPConnection -LocalPort 16680 -ErrorAction SilentlyContinue
Get-Content .\server\local-server\deployment\logs\bootstrap.log -Tail 80
Get-Content .\server\local-server\deployment\logs\server.log -Tail 80
```

`Setup-Runtime.cmd` 会联网获取并自动检查清单指定的 CPython 嵌入式运行时，不会覆盖已有 `runtime/windows` 目录。已有但损坏的运行时应先保留诊断信息，再从可信完整包恢复。

插件通过代理 DLL 加载到游戏进程，安全软件可能拦截相关文件。先核对来源、ZIP 校验值和具体隔离记录，再决定如何处理；不要为来源不明的 DLL 关闭防护。

恢复官方客户端时先执行 `Uninstall-Client.cmd`，确认 `d3dcompiler_47.dll` 还原。若仍异常，使用安装器记录中的原始备份或官方启动器修复文件；确认无需回退之前保留客户端备份目录。

## 10. 问题反馈

提交 Issue 时，建议提供以下最小信息：

- 发布包版本、目标游戏版本及安装器的成功／失败提示。
- Windows／Linux 环境、本机或局域网模式、是否使用自定义端口与状态目录。
- 从何处开始、执行了哪些步骤、预期与实际结果、是否能稳定复现。
- 异常发生于安装、活动入口、准备、战斗、协防、重连还是结算阶段。
- 脱敏后的 `/health` 字段、相关日志片段或原生诊断 JSON。

不要上传官方账号信息、`operator.token`、参与者凭据、房间码、完整数据库或整个客户端备份目录。旧配置备份可能含有用户自行填写的令牌；网关日志、截图和诊断文件也应检查后再分享。

## 11. 许可与第三方声明

本项目自行编写的代码按 **GNU GPL v3.0 or later（GPL-3.0-or-later）** 发布；许可证全文见 [`LICENSE`](LICENSE)，具体文件及第三方组件以相应声明为准。

《明日方舟》及「卫戍协议」相关名称、角色、美术、模型、界面、音乐、音效、文本、游戏数据，以及从官方客户端提取或派生的资源，其权利属于原权利人，**不在本项目代码 GPL 授权范围内**。不得将完整包用于商业运营、付费开服、广告变现、付费下载或其他盈利活动。

| 组件 | 用途 | 许可材料位置 |
| --- | --- | --- |
| MinHook | Windows 原生 Hook | 包内 `client/MinHook.LICENSE.txt`，BSD 2-Clause |
| CPython | Windows 服务端运行时 | [`CPython 许可副本`](deploy/server/licenses/CPython-3.13.16-LICENSE.txt)；完整运行时声明见包内 `server/runtime/windows/LICENSE.txt` |
| Caddy | Linux 反向代理 | [`LICENSE`](deploy/server/licenses/Caddy-2.11.7-LICENSE.txt)、[`AUTHORS`](deploy/server/licenses/Caddy-2.11.7-AUTHORS.txt) |
| Python／Caddy 官方容器镜像 | Linux 部署基础环境 | 保留各镜像所含组件的许可；使用部署向导记录的固定版本 |

仓库归档的许可来源见 [`deploy/server/licenses/sources.json`](deploy/server/licenses/sources.json)。项目按现状提供，不附带功能完整性、特定客户端版本兼容性或持续可用性保证。如相关权利人认为仓库包含不应分发的内容，可通过项目 Issue 联系维护者处理。
