# openmediavault-example — OMV 8.x 插件模板骨架

本仓库是 omv-plugins 工作区的**插件模板**：对标 1Panel 应用商店（表单参数化 → 统一生命周期编排）与飞牛 fnOS fpk 包（原生包生命周期 + 标准化目录 + 控制接口）的优点，落到 OMV 自身体系（deb 包 + confdb + Salt + Workbench + ctl 脚本）——fpk 的生命周期脚本本就是仿 deb 的，1Panel 的表单参数化本就是 datamodel+Salt 渲染的弱化版，OMV 体系两者天然覆盖，缺的只是成文规范（见 omv-plugin-dev skill `references/integration-modes.md` 的三轨模型）。结构提炼自 frpc（systemd 服务模式）、immich（Docker Compose 模式）、1panel（RPC 后台任务/按钮模式）等已验证插件，可直接复制派生新插件。

> 模板代码本身是「完整可编译的示例插件」，但 **example 服务并不存在**，不要把本模板直接装到任何 NAS 上；先按下面流程派生、再进测试环境验证。

## 目录结构

```
omv-example/
├── .github/workflows/build.yml        # CI：lint（php/yaml/json/sh）+ dpkg-buildpackage + Release
├── .gitattributes                     # 强制 LF（Windows 开发的生命线，勿动）
├── .gitignore
├── render-vars.sh                     # 打包前渲染 ${GITHUB_USER} 等构建期占位符
├── debian/                            # deb 打包套件
│   ├── control / changelog / compat(12) / copyright(GPL-3+) / source/format
│   ├── rules                          # 修复 Windows 丢失的可执行位
│   ├── variables.env                  # 构建期变量（GITHUB_USER）
│   ├── openmediavault-example.install     # 文件安装清单
│   ├── openmediavault-example.postinst    # chmod + update-workbench + confdb create/migrate
│   ├── openmediavault-example.postrm      # purge 清 config + systemd 渲染物（数据保留）
│   ├── openmediavault-example.prerm
│   └── openmediavault-example.triggers    # activate restart-engined
├── srv/salt/omv/deploy/example/
│   ├── init.sls                       # 注册 jinja 过滤器 + pillar 分发
│   ├── default.sls                    # 双模式 state（mode 切换 systemd/compose 分支）
│   └── files/
│       ├── example.conf.j2            # systemd 模式：服务配置
│       ├── example.service.j2         # systemd 模式：unit 文件
│       ├── example.env.j2             # compose 模式：.env（0600）
│       └── example.compose.yml.j2     # compose 模式：栈文件
├── usr/sbin/omv-example-ctl           # 双模式控制脚本（RPC 后台任务调用）
└── usr/share/openmediavault/
    ├── datamodels/conf.service.example.json   # 配置模型（禁 required、单一类型）
    ├── confdb/create.d/conf.service.example.sh # 安装时建配置节点（默认值须与 datamodel 一致）
    ├── engined/rpc/example.inc        # RPC：Example（get/set/start/stop/restart/getLog）
    ├── engined/module/example.inc     # 服务面板状态 + conf 变更置脏
    ├── locale/                        # pot + zh_CN + zh_TW
    └── workbench/
        ├── navigation.d/services.example(.settings).yaml
        ├── route.d/services.example(.settings).yaml   # 设置页必须 editing: true
        └── component.d/omv-services-example-*.yaml    # 导航页 + 设置表单页
```

## 派生新插件（五步）

### 1. 复制并重命名

复制 `omv-example` 为新的一级文件夹（= 独立 GitHub 仓库），如 `omv-foo`。

> 命名注意：服务标识不能以数字开头（1panel 内部因此叫 `onepanel`）；RPC 服务名不得与核心服务冲突（用 `omv-rpc` 列表核对，如管理器插件用 `PkgMgr` 而非 `PluginMgr`）。

### 2. 全局替换 example（大小写变体一次理清）

| 位置 | 模板取值 | 替换规则 |
|---|---|---|
| 仓库名/文件夹 | `omv-example` | `omv-<name>` |
| 包名 | `openmediavault-example` | `openmediavault-<name>` |
| 服务标识（datamodel id、xpath、salt 目录、ctl 内 CONF_XPATH） | `example` | `<name>` |
| RPC 服务名（`getName()`、表单 `service:`） | `Example` | `<Name>`（首字母大写） |
| RPC 类名 | `OMVRpcServiceExample` | `OMVRpcService<Name>` |
| engined module 类名 | `Example` | `<Name>` |
| 控制脚本 | `omv-example-ctl` | `omv-<name>-ctl`（同步 .gitattributes、rules、CI） |
| Workbench 组件名 | `omv-services-example-*` | `omv-services-<name>-*` |
| 通知对象 | `org.openmediavault.conf.service.example` | `...conf.service.<name>` |
| UI 显示名 | `Example` / `示例` | 目标服务名（.po 同步改） |

### 3. 选集成轨并裁剪（三选一，选轨决策树见 skill `references/integration-modes.md`）

| 上游发布形态 | 保留/做法 | 参照插件 |
|---|---|---|
| 只发 Docker 镜像 | compose 模式 | omv-immich |
| 发 Linux 原生二进制/脚本 | systemd 模式 | omv-frpc / omv-clouddrive2 |
| 有官方安装器/自更新器 | 模板两轨都不留，ctl 重写为安装器轨：下载+校验+官方脚本静默安装（Docker/daemon.json 全禁碰），Salt 只管官方 unit 的 enabled/running（onlyif 未装跳过），升级走官方 updater，卸载自写非交互且保数据 | omv-1panel |

- **systemd 模式**（frpc/clouddrive2 类宿主机服务）：保留 `default.sls` 的 `{% else %}` 分支 + `example.conf.j2` / `example.service.j2`；替换 `ExecStart` 为真实上游命令、`example.conf.j2` 为真实配置格式。删除 compose 分支、`*.env.j2` / `*.compose.yml.j2`、`mode` 字段与表单「Compose mode」区。
- **compose 模式**（immich 类容器应用）：保留 compose 分支 + `example.env.j2` / `example.compose.yml.j2`；替换服务定义。删除 systemd 分支、`example.conf.j2` / `example.service.j2`、`mode` 字段与表单「Systemd mode」区；`postrm` 里的 unit 清理也一并删。
- 保留单分支后，`default.sls` 里 `{% if mode == ... %}` 可简化为直接写状态；ctl 里对应分支同理。

### 4. 改元数据与字段

- `debian/control`：Description 改成真实描述；依赖按需加。
- `debian/changelog`：首条 8.0.1 改为真实描述（版本号规则：大版本与 OMV 一致 `8.y`，从最小号递增；Release tag = `v` + 同版本号）。
- datamodel / confdb create.sh / RPC `$settingsFields` / 表单字段：**四处必须同步**增删字段；datamodel 禁 `required`、禁 union 类型、字段名避开保留键（`user`/`group`/`service`/`share` 等，全表见 omv-plugin-dev skill pitfalls.md）。
- 升级已有插件版本时，新增字段必须配套 `usr/share/openmediavault/confdb/migrations.d/conf.service.<name>_8.0.x.sh`。

### 5. 自检 → 发布 → 验证

```bash
# 本地（MSYS2 bash）
sh -n render-vars.sh usr/sbin/omv-foo-ctl debian/*.postinst debian/*.postrm debian/*.prerm usr/share/openmediavault/confdb/create.d/*.sh
python -c "import json;json.load(open('usr/share/openmediavault/datamodels/conf.service.foo.json'))"
```

推送 GitHub 后打 tag `v8.0.1` → CI（lint + build + Release 附 deb）→ 从 Release 下 deb 装测试环境验证（生产 NAS 禁装未验证插件）：

1. `dpkg -i` / `apt-get install ./...deb` 装上后 Ctrl+F5 强刷 WebUI（Workbench 只在启动时拉一次 route-config.json）。
2. `omv-rpc '<Name>' get '{}'` 验后端；页面改设置 → 应用 → 验 Salt 渲染物（unit / compose 文件）与启停往返。
3. 中文界面逐项过一遍 i18n。

## 内置的已验证模式（派生时保留，勿随手删）

- **RPC**：`set()` 用 `array_intersect_key` 白名单剥离只读字段；启停/日志走 `execBgProc` 后台任务（taskDialog 实时输出）；`get()` 实时拼状态字段，状态文本返回英文 msgid 由前端 `translate` 过滤。
- **表单页**：只读状态字段必须 `submitValue: false` + `readonly: true`（否则 getFormValues 会把派生值回写给 set）；设置页 route 必须带 `editing: true`（否则详情页数据不加载）；「打开面板」按钮的 `externalRedirect` + `location() | get('hostname')` 写法可直接复用。
- **Salt**：`file.managed` 显式 `- template: jinja`；互斥 if/else 分支的 state ID 必须不同；`onlyif/unless` 防目标不存在时 Apply 失败；渲染物头部带 auto-generated 标记（禁手改，改模板）。
- **目录与版本**：目录类字段（`composeDirRef`）用 **sharedfolder 引用**——datamodel `oneOf: uuidv4|空串` + 表单 `sharedFolderSelect` 下拉（用户像飞牛选卷一样选共享文件夹）+ Salt `omv_conf.get_sharedfolder_path`（**必须 `{% if ref %}` 守卫**）+ ctl `omv_get_sharedfolder_path`（helper-functions 自带）+ engined module 监听 `org.openmediavault.conf.system.sharedfolder`（否则共享文件夹变更后不重渲染）。数据一律落阵列，禁放 `/root/`；`image`/`version` 字段固定上游版本，**禁 latest**（不可复现、无法检测更新）；更新检测 RPC 比对上游 Releases，参照 omv-immich / omv-1panel 的 `getLatestVersion`（GitHub 匿名 403 限流要有降级提示）。
- **构建**：`.gitattributes` 强制 LF；`render-vars.sh` 渲染 + 残留校验；CI 校验 changelog 与 tag 一致。
- 禁改上游服务自身功能：插件只做 OMV 侧集成（启停/自启/数据目录/状态展示），上游业务在其原生界面管理。

## 更多参考

- 完整工作流与坑清单：`.codebuddy/skills/omv-plugin-dev/`（SKILL.md + references/{architecture,integration-modes,ci-packaging,i18n-workbench,pitfalls}.md）
- 三轨集成模式设计（对标 1Panel/fnOS 的成文规范）：`.codebuddy/skills/omv-plugin-dev/references/integration-modes.md`
- Workbench 字段/组件手册：`omv-reference/omv-workbench-docs/`
- 运维红线（装插件到 NAS 的流程）：`project_rules_ops.md` + omv-safe-admin skill
