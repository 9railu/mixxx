# RAIDJ 依存ライブラリ管理ファイル
# 競合確認・ライセンス確認・導入方法の一覧

---

## 見方

| 列 | 内容 |
|---|---|
| 導入方法 | vcpkg / 手動ビルド / pip / submodule / 同梱 / システム |
| リンク方式 | 静的（.lib）/ 動的（.dll）/ プラグイン / 外部プロセス |
| 競合リスク | 既知の競合・注意事項 |

---

## C++ ライブラリ

| ライブラリ | バージョン | ライセンス | 導入方法 | リンク方式 | 用途 | 競合リスク |
|---|---|---|---|---|---|---|
| **Mixxx 2.6** | 2.6.x | GPL v2 | フォーク | — | ベース | — |
| **Qt6** | 6.x | LGPL v3 | Mixxx公式スクリプト | 動的 | UI・スレッド・ネットワーク全般 | — |
| **FFmpeg** | 6.x以上 | LGPL v2.1 | Mixxx公式スクリプト | 動的 | 映像処理・音声デコード | Essentia内FFmpegと分離済み（★1） |
| **Essentia** | latest cmake branch | AGPL v3 | 手動（wo80フォーク） | **静的** | BPM/Key解析・BeatTrackerMultiFeature | FFmpegバージョン差異を静的リンクで回避（★1） |
| **ONNX Runtime** | latest | MIT | vcpkg | 動的 | Demucsステム分離推論 | Qt6スレッドとは分離済み・実行はQThreadPool |
| **SQLite3** | Mixxx同梱版 | Public Domain | Mixxx公式スクリプト | 静的 | mixxxdb.sqlite（Mixxx内部DB） | SQLCipherと同時リンク不可（★2） |
| **SQLCipher** | latest | BSD-style | vcpkg | 静的 | Rekordbox master.db復号 | SQLite3と同時リンク不可（★2） |
| **qsqlcipher-qt6** | latest | LGPL | 手動ビルド | プラグイン（.dll） | Qt SQL ドライバー差し替え | ★2の解決策。QSQLITEドライバーを置き換える |
| **OpenSSL** | latest | Apache 2.0 | vcpkg | 動的 | qsqlcipher-qt6の依存 | — |
| **NDI SDK** | 6.x | 独自（非商用無償） | 手動インストール | 動的 | 映像+音声のOBSへの送出 | 将来ライセンス変更リスクあり |
| **GL Transitions** | latest | MIT | git submodule | — （GLSLファイルのみ） | VJ映像クロスフェード（121種シェーダー） | — |
| **Qt6-WebSockets** | Qt6同梱 | LGPL v3 | Mixxx公式スクリプト or vcpkg | 動的 | OBS WebSocket クライアント（QWebSocket） | Mixxx公式スクリプトに含まれるか要確認（★3） |
| **Qt6-Network** | Qt6同梱 | LGPL v3 | Mixxx公式スクリプト or vcpkg | 動的 | Google Drive REST API（QNetworkAccessManager） | 同上（★3） |
| **Qt6-Multimedia** | Qt6同梱 | LGPL v3 | Mixxx公式スクリプト or vcpkg | 動的 | VJ背景動画再生（QMediaPlayer）・Windowsコーデック注意 | 同上（★3） |
| **Qt Keychain** | latest | LGPL | vcpkg | 動的 | Google Driveトークンの安全保存 | — |
| **FFTW3** | latest | GPL v2+ | Mixxx公式スクリプト / Essentiaスクリプト | 静的 | Essentiaのビルド依存・FFT計算 | Essentia内部で使用。Mixxx外部には露出しない |
| **VJフォールバックGLSL** | — | 要確認（★4） | 手動取得・同梱 | — （GLSLファイルのみ） | 映像なし時のビジュアライザー | 取得元・ライセンス未確定（★4） |
| **VST3 SDK** | latest | GPL v3 | 手動（フェーズ4） | 静的 | VST3プラグインホスト | GPL v3×Mixxx GPL v2（個人利用のため問題なし） |

---

## 外部 API（オプション）

| サービス | ライセンス | 用途 | 競合リスク |
|---|---|---|---|
| **LALAL.AI API** | 商用サービス（従量課金） | 高品質10ステム分離（オプション。未設定時はDemucsで動作） | APIキー未設定時は機能無効。費用・サービス停止リスクあり |

---

## Python パッケージ（requirements.txt）

| パッケージ | バージョン | ライセンス | 用途 | 競合リスク |
|---|---|---|---|---|
| **yt-dlp** | latest | Unlicense | YouTube音声・映像取得 | YouTube仕様変更で突然停止リスクあり |
| **pyrekordbox** | latest | MIT | SQLCipher復号キーの参照元 | Rekordbox DBスキーマ変更リスクあり |

---

## AI モデル

| モデル | ライセンス | 取得元 | 用途 | 競合リスク |
|---|---|---|---|---|
| **Demucs v4 ONNX** | MIT | Mixxx GSoC 2025 GitHub Releases（非公開時はHugging Face） | ステム分離 | ONNXRuntimeバージョン固定が必要 |

---

## 外部ツール・SDK

| ツール | ライセンス | 取得方法 | 用途 | 競合リスク |
|---|---|---|---|---|
| **Python 3.10+** | PSF | システムインストール | yt-dlp・pyrekordbox実行環境 | — |
| **pip** | MIT | Python同梱 | Pythonパッケージ管理 | — |

---

## 既知の競合と解決策

### ★1：Essentia × FFmpeg バージョン差異

| 項目 | 内容 |
|---|---|
| 問題 | EssentiaとMixxxが異なるバージョンのFFmpegを参照するとDLL競合が発生する |
| 解決 | Essentiaを**スタティックビルド**（--static）。FFmpegをEssentia内部に静的リンクし、MixxxのFFmpegとを分離 |
| 手順 | `BUILD_SETUP.md` Step 5 参照 |
| 状態 | ✅ 対処済み |

### ★3：Qt6 追加モジュールの Mixxx 公式スクリプト収録確認（未解決）

| 項目 | 内容 |
|---|---|
| 問題 | qt6-websockets / qt6-network / qt6-multimedia が Mixxx の `buildenv.py` に含まれているか未確認 |
| 影響 | 含まれていない場合、vcpkg で別途インストールが必要 |
| 対処 | フォーク後に `tools/buildenv.py` の内容を確認して判断 |
| 状態 | ⚠️ 未確認 |

### ★4：VJ フォールバック GLSL シェーダーの取得元（未決定）

| 項目 | 内容 |
|---|---|
| 問題 | ShaderToy 互換シェーダーをどこから取得・同梱するか未決定。ライセンスも要確認 |
| 候補 | glslsandbox.com / Shadertoy（CC BY-NC-SA）/ 自作 GLSL |
| 注意 | ShaderToy のシェーダーは CC BY-NC-SA のため商用不可だが個人利用は可能 |
| 状態 | ⚠️ 未決定 |

### ★2：SQLCipher × SQLite3 シンボル競合

| 項目 | 内容 |
|---|---|
| 問題 | SQLCipherとSQLite3は`sqlite3_*`シンボルを完全共有。同時リンクでリンクエラー発生 |
| 解決 | `qsqlcipher-qt6`プラグインでQSQLITEドライバーをSQLCipherベースに差し替え。SQLCipherはSQLite3の完全上位互換のためmixxxdb.sqliteも暗号化なしで開ける |
| 手順 | `BUILD_SETUP.md` Step 4 参照 / `tasks/TASK_sqlcipher_conflict.md` 参照 |
| 状態 | ✅ 対処済み |

---

## ライセンスまとめ（個人利用・配布なし）

| ライセンス種別 | 該当ライブラリ | 個人利用の可否 |
|---|---|---|
| GPL v2 | Mixxx | ✅ 配布しないため問題なし |
| AGPL v3 | Essentia | ✅ 配布しないため問題なし |
| GPL v3 | VST3 SDK | ✅ 配布しないため問題なし |
| LGPL | Qt6・Qt Keychain・qsqlcipher-qt6 | ✅ |
| MIT | ONNX Runtime・GL Transitions・pyrekordbox | ✅ |
| Apache 2.0 | OpenSSL | ✅ |
| BSD-style | SQLCipher | ✅ |
| Unlicense | yt-dlp | ✅ |
| 独自（非商用無償） | NDI SDK | ✅ 非商用・個人利用 |

**結論：全ライブラリ、個人利用・配布なしの範囲で問題なし。**

---

## 新規ライブラリ追加時のチェックリスト

新しいライブラリを追加する際は以下を確認してこのファイルに追記する。

```
□ ライセンスは個人利用で問題ないか
□ 既存ライブラリとシンボルが重複しないか
  → 特に SQLite3系・FFmpeg系・OpenSSL系は要注意
□ リンク方式（静的/動的）は既存構成と整合しているか
□ スレッド安全か（オーディオスレッドに干渉しないか）
□ Windows MSVC でビルド可能か
□ BUILD_SETUP.md に追加手順を記載したか
□ RISKS.md に該当リスクを記載したか
```

---

最終更新：2026-06-05
