# RAIDJ ビルド環境構築手順書
# Windows / MSVC + vcpkg / Mixxx 2.6 フォークベース

---

## 前提条件

| ツール | バージョン | 備考 |
|---|---|---|
| Windows | 11 | 確認済み環境 |
| Visual Studio | 2022 以上 | C++ ワークロード必須 |
| Git | 最新 | |
| CMake | 3.21 以上 | VS インストーラーから入れられる |
| Python | 3.10 以上 | yt-dlp・pyrekordbox 用 |
| GitHub Copilot CLI | 1.0.54（`copilot` コマンドで使用） | 導入済み・動作確認済み |

---

## Step 1：Visual Studio のセットアップ

Visual Studio Installer を開き、以下のワークロードにチェックを入れてインストール。

```
☑ C++ によるデスクトップ開発
  └ 個別コンポーネント：
    ☑ MSVC v143（最新）
    ☑ Windows 11 SDK（最新）
    ☑ CMake ツール for Visual Studio
    ☑ vcpkg パッケージマネージャー（統合）
```

---

## Step 2：vcpkg のセットアップ

```powershell
# vcpkg を任意の場所にクローン（例：C:\vcpkg）
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Visual Studio との統合
.\vcpkg integrate install
```

環境変数を設定（システム環境変数に追加）：
```
VCPKG_ROOT = C:\vcpkg
```

---

## Step 3：Mixxx 2.6 のフォーク・クローン

> ⚠️ Mixxx 2.6 は現時点（2026年6月）でベータ版。最新安定版は 2.5.6。
> 個人利用フォークのため 2.6 beta ブランチをベースとして進める。

GitHub 上で `mixxxdj/mixxx` を自分のアカウントにフォークしてから：

```powershell
git clone https://github.com/<your-account>/mixxx.git RAIDJ
cd RAIDJ

# 2.6 ブランチに切り替え（beta ブランチ）
git checkout 2.6

# RAIDJ 開発ブランチを作成
git checkout -b raidj-main
```

**プロジェクトドキュメントをリポジトリに配置：**
```powershell
# 事前に作成済みのドキュメントをリポジトリ直下にコピー
# （AGENTS.md・RAIDJ_PROJECT.md・MODULE_DESIGN.md 等）
Copy-Item C:\Users\ts060\claude\djcontroler\*.md RAIDJ\
Copy-Item C:\Users\ts060\claude\djcontroler\requirements.txt RAIDJ\
New-Item -ItemType Directory -Force RAIDJ\tasks
Copy-Item C:\Users\ts060\claude\djcontroler\tasks\*.md RAIDJ\tasks\
git add *.md requirements.txt tasks\
git commit -m "Add RAIDJ project documentation"
```

---

## Step 4：Mixxx 公式スクリプトで標準依存をインストール

> ⚠️ Mixxx は独自の依存管理スクリプトを使用している。vcpkg と混在させると競合するため、
> Mixxx 標準依存は公式スクリプト経由でインストールし、RAIDJ 追加分のみ vcpkg で管理する。

```powershell
cd RAIDJ

# Mixxx 公式の Windows 依存ビルドスクリプトを実行
# （Qt6・portaudio・libsndfile・taglib・FFmpeg 等を一括取得）
python tools/buildenv.py --download-only
```

完了後、`build/` 以下に依存ライブラリが展開される。

**RAIDJ 追加依存のみ vcpkg で管理：**
```powershell
cd C:\vcpkg

# ONNX Runtime（Demucs ステム分離）
.\vcpkg install onnxruntime:x64-windows

# OpenSSL（qsqlcipher-qt6 のビルドに必須）
.\vcpkg install openssl:x64-windows

# SQLCipher（Rekordbox master.db の暗号化解除）
# ⚠️ 通常の SQLite3 と同時リンク不可。qsqlcipher-qt6 プラグインで差し替える方式を採用
.\vcpkg install sqlcipher:x64-windows

# qsqlcipher-qt6（Qt6 の SQLite ドライバーを SQLCipher に差し替え）
# → mixxxdb.sqlite（暗号化なし）と master.db（暗号化あり）を同一ドライバーで扱える
git clone https://github.com/chehrlic/qsqlcipher-qt6.git C:\qsqlcipher-qt6
cd C:\qsqlcipher-qt6
cmake -B build `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Qt Keychain（Google Drive トークン保存）
.\vcpkg install qtkeychain:x64-windows
```

---

## Step 5：Essentia のビルド（Windows 手動・スタティックビルド推奨）

> ⚠️ 既知問題：Essentia を DLL（動的）ビルドすると `swresample-1.dll` が実行時に
> 見つからずクラッシュする（Issue #1310）。スタティックビルドで回避する。
> また標準 CMake は VS2022 非対応のため、cmake 対応フォークを使用する。

```powershell
# cmake 対応フォーク（VS2022 対応済み）を使用
git clone https://github.com/wo80/essentia.git -b cmake C:\essentia
cd C:\essentia

# スタティックビルドで DLL 依存問題を回避
.\packaging\build-dependencies-msvc.bat --static --build-type Release

mkdir build && cd build
cmake .. `
  -DCMAKE_BUILD_TYPE=Release `
  -DBUILD_SHARED_LIBS=OFF `
  -DESSENTIA_STATIC=ON `
  -DWITH_PYTHON=OFF

cmake --build . --config Release
```

ビルド後、`essentia.lib` と `include/` を RAIDJ の CMakeLists.txt から参照できる場所に配置。

---

## Step 6：NDI SDK のインストール

1. [ndi.video/tools/](https://ndi.video/tools/) から NDI SDK for Windows をダウンロード
2. インストーラーを実行（デフォルト設定で OK）
3. インストール先（通常 `C:\Program Files\NDI\NDI 6 SDK`）を環境変数に追加：
   ```
   NDI_SDK_DIR = C:\Program Files\NDI\NDI 6 SDK
   ```

---

## Step 7：Python 環境のセットアップ

`requirements.txt` で一括管理する。

```powershell
# RAIDJ/requirements.txt を作成（リポジトリに含める）
Set-Content RAIDJ\requirements.txt @"
yt-dlp
pyrekordbox
"@

# 一括インストール
pip install -r RAIDJ\requirements.txt

# 動作確認
yt-dlp --version
python -c "import pyrekordbox; print('pyrekordbox OK')"
```

> 今後 Python パッケージが増えた場合は `requirements.txt` に追記する。

---

## Step 8：Demucs モデルの手動取得（開発時）

アプリ起動時は自動 DL だが、開発中は手動で配置する。

```powershell
# モデル保存先ディレクトリを作成
New-Item -ItemType Directory -Force "$env:APPDATA\RAIDJ\models"

# Mixxx GSoC 2025 GitHub Releases から ONNX 変換済みモデルを取得
# ※ URL は Mixxx GSoC 2025 リポジトリの Releases ページで確認
# 非公開の場合は Hugging Face Hub からフォールバック：
#   pip install huggingface_hub
#   python -c "from huggingface_hub import hf_hub_download; hf_hub_download('facebook/demucs', 'htdemucs.onnx', local_dir='$env:APPDATA\RAIDJ\models')"

# 配置確認
ls "$env:APPDATA\RAIDJ\models"
```

---

## Step 9：RAIDJ の CMakeLists.txt に追加依存を記述

`CMakeLists.txt` に以下を追加（Mixxx 標準の記述に続けて）：

```cmake
# FFmpeg（Mixxx 公式スクリプトで取得済み分を参照）
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavfilter libswscale)

# ONNX Runtime
find_package(onnxruntime REQUIRED)

# Essentia（手動スタティックビルド分）
include_directories(C:/essentia/src)
link_directories(C:/essentia/build/src/Release)
target_link_libraries(raidj PRIVATE essentia)

# NDI SDK
include_directories($ENV{NDI_SDK_DIR}/Include)
link_directories($ENV{NDI_SDK_DIR}/Lib/x64)
target_link_libraries(raidj PRIVATE Processing.NDI.Lib.x64)

# OpenSSL（qsqlcipher-qt6 の依存）
find_package(OpenSSL REQUIRED)
target_link_libraries(raidj PRIVATE OpenSSL::Crypto)

# qsqlcipher-qt6 プラグイン（Qt SQL ドライバーとして動的ロード）
# ビルド後の qsqlcipher.dll を Qt プラグインディレクトリに配置
# → cmake install ステップで自動コピーするよう設定

# Qt Keychain
find_package(Qt6Keychain REQUIRED)
target_link_libraries(raidj PRIVATE Qt6Keychain)
```

---

## Step 10：初回ビルドと動作確認

```powershell
cd RAIDJ
mkdir build && cd build

cmake .. `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DCMAKE_BUILD_TYPE=Debug `
  -G "Visual Studio 17 2022" -A x64

cmake --build . --config Debug

# 起動確認
.\Debug\raidj.exe
```

ビルドが通り、Mixxx 2.6 ベースの UI が起動すれば環境構築完了。

---

## Step 11：GL Transitions の組み込み

```powershell
# GL Transitions リポジトリをサブモジュールとして追加
cd RAIDJ
git submodule add https://github.com/gl-transitions/gl-transitions.git src/vj/gl-transitions
git submodule update --init
```

GLSL ファイル群（`transitions/*.glsl`）をビルド時にリソースとしてバンドルする。

---

## 別 PC での開発継続手順

別 PC でゼロから開発を再開する場合の最短手順。

### 1. 必須ツールのインストール
```powershell
# GitHub CLI
winget install --id GitHub.cli

# Git（未インストールの場合）
winget install --id Git.Git

# Python 3.10 以上（未インストールの場合）
winget install --id Python.Python.3.11

# Claude Code（claude.ai からインストール）または
# GitHub Copilot CLI
winget install --id GitHub.cli  # gh 経由で copilot も使用可能
```

### 2. GitHub 認証
```powershell
gh auth login
# GitHub.com / HTTPS / Login with a web browser を選択
```

### 3. ドキュメントリポジトリのクローン
```powershell
gh repo clone 9railu/RAIDJ-docs
cd RAIDJ-docs
```

### 4. Claude Code / Copilot CLI でそのまま開発再開
```powershell
# Claude Code（claude.ai/code からインストール済みの場合）
claude .

# または Copilot CLI
copilot
# → AGENTS.md を自動で読み込みプロジェクトコンテキストを把握
```

> AGENTS.md がリポジトリに含まれているため、
> Claude Code・Copilot CLI どちらもプロジェクトの全コンテキストを
> 初回から自動で把握して開発を継続できる。

### 5. ドキュメント更新後のプッシュ
```powershell
git add .
git commit -m "更新内容の説明"
git push
```

---

## ブランチ運用方針

```
raidj-main          ← 安定動作する状態を常に保つ
  └ feature/xxxx    ← 機能ごとに切って開発
                       完成したら raidj-main にマージ
```

機能ブランチの命名例：
```
feature/rekordbox-import
feature/youtube-integration
feature/stem-separation
feature/vj-engine
```

---

## トラブルシューティング

| 症状 | 対処 |
|---|---|
| `qt6 not found` | `vcpkg install qt6-base:x64-windows` を再実行 |
| Essentia ビルドエラー | FFTW3 が vcpkg で入っているか確認 |
| NDI が見つからない | `NDI_SDK_DIR` 環境変数を確認・再起動 |
| `yt-dlp` コマンドが見つからない | Python の Scripts フォルダを PATH に追加 |
| CMake が MSVC を見つけない | VS の「開発者コマンドプロンプト」から cmake を実行 |
| qsqlcipher-qt6 ビルドエラー | OpenSSL が vcpkg でインストールされているか確認 |
| `OpenSSL::Crypto` が見つからない | `.\vcpkg install openssl:x64-windows` を再実行 |
| Demucs モデルが見つからない | `%APPDATA%\RAIDJ\models\` に .onnx ファイルがあるか確認 |

---

最終更新：2026-06-05（Mixxx 2.6 ベータ版注記・ドキュメント配置手順追加）
