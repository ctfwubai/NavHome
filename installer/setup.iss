; 导航之家（NavHome）安装脚本
; 要求：所有用户可用（含 AD 域普通用户）、默认安装到 D 盘、重装后可重新导入恢复

#define MyAppName "导航之家"
#define MyAppVersion "2.3.0"
#define MyAppPublisher "NavHome"
#define MyAppExeName "NavHome.exe"

[Setup]
; 唯一标识（升级时保持不变）
AppId={{A1B2C3D4-E5F6-4A7B-8C9D-0E1F2A3B4C5D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
; 默认安装到 D 盘
DefaultDirName=D:\NavHome
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
; 管理员权限安装 → 程序写入公共目录，普通用户（含 AD 域）均可运行
PrivilegesRequired=admin
; 仅 64 位
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=.
OutputBaseFilename=NavHome-Setup-{#MyAppVersion}-20261101
Compression=lzma
SolidCompression=yes
WizardStyle=modern
SetupIconFile=icons\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
; 用户数据存于 %APPDATA%\NavHome（不在安装目录），卸载/重装不丢失

[Languages]
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; 主程序 + Qt 运行库 + 插件
Source: "dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; 20 个可选应用图标
Source: "icons\icon_*.ico"; DestDir: "{app}\icons"; Flags: ignoreversion

[Icons]
; 所有用户开始菜单 / 桌面快捷方式
Name: "{commonstartmenu}\{#MyAppName}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{commonstartmenu}\{#MyAppName}\卸载 {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
