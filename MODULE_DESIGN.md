# RAIDJ モジュール設計メモ
# Mixxx 2.6 フォークにおける追加機能の設計方針

---

## 基本方針

- **既存クラスの継承・拡張を優先**する。Mixxx の設計パターンに沿って追加することでコードの一貫性を保つ
- **新機能は独立したクラスに分離**し、既存コードへの影響範囲を最小化する
- **スレッド安全**：オーディオスレッドへの干渉は厳禁。追加処理は必ず別スレッドで実行
- **設定値は `UserSettingsPointer`（mixxx.cfg）** に集約する

---

## Mixxx の主要クラス構造（参考）

```
MixxxMainWindow
  ├── ControllerManager      ← MIDI/HID コントローラー管理
  ├── Library                ← ライブラリ管理のルート
  │   ├── LibraryFeature[]   ← 各ライブラリパネル（iTunes, Rekordbox 等）
  │   └── TrackDAO           ← SQLite（mixxxdb）へのアクセス層
  ├── PlayerManager          ← デッキ（BaseTrackPlayer）管理
  ├── AnalyzerManager        ← BPM/Key 解析パイプライン
  ├── EffectsManager         ← エフェクト管理
  └── SoundManager           ← オーディオ入出力
```

---

## 各追加機能のモジュール設計

> **波形カラー化**：`src/waveform/renderers/WaveformRendererRGB`（既存）を拡張。
> 高精細化・ズーム品質のみ追加実装。ゼロからの実装不要。

> **yt-dlp**：Python パッケージ。起動時に `pip install -U yt-dlp` を QProcess で実行。

> **Demucs モデル**：Mixxx GSoC 2025 GitHub Releases から QNetworkAccessManager でDL。
> 非公開時は Hugging Face Hub からフォールバック。

---

### 1. Rekordbox インポート

**追加クラス：** `RekordboxDirectFeature`
**継承元：** `LibraryFeature`
**追加場所：** `src/library/rekordboxdirectfeature.h/.cpp`（新規）

```
Library
  └── RekordboxDirectFeature（新規）
        ├── RekordboxDBReader       ← C++ + SQLCipher で master.db を直接読み取り
        │                             （復号キーは pyrekordbox ソースから取得）
        ├── RekordboxCueImporter    ← ホットキュー・メモリキュー変換
        └── RekordboxGridImporter   ← ビートグリッド変換
```

**設計ポイント：**
- 既存の `RekordboxFeature`（USB読み込み）と干渉しないよう別クラスとして分離
- インポート処理は `QThread` で非同期実行。UI をブロックしない
- キュー・ビートグリッドは Mixxx 標準の `CueDAO` / `BeatsDAO` 経由で保存
- SQLCipher を vcpkg でインストール済み（`BUILD_SETUP.md` 参照）

---

### 2. YouTube 統合

**追加クラス：** `YouTubeFeature`, `YouTubeDownloader`, `YouTubeCacheManager`
**継承元：** `LibraryFeature`
**追加場所：** `src/library/youtube/`（新規ディレクトリ）

```
Library
  └── YouTubeFeature（新規）
        ├── YouTubeSearchModel      ← 検索結果の Qt モデル
        ├── YouTubeDownloader       ← yt-dlp を QProcess で呼び出し
        │     └── ProgressSignal    ← プログレスバーへの通知
        ├── YouTubeCacheManager     ← 一時ファイル管理（10GB 上限・お気に入り）
        └── YouTubeCueStore         ← 動画 ID をキーに mixxxdb へキュー保存
```

**設計ポイント：**
- `yt-dlp` は `QProcess` で呼び出し（外部プロセス）。C++ 内に埋め込まない
- ダウンロード完了後は `PlayerManager` の `loadTrackToPlayer()` を呼び出してデッキへロード
- 動画 ID（例：`dQw4w9WgXcQ`）を `TrackDAO` のカスタムカラムに保存してキュー紐付け
- お気に入りフラグも `TrackDAO` に追加カラムとして保持

---

### 3. BPM・キー解析（Essentia 統合）

**追加クラス：** `AnalyzerEssentiaBPM`, `AnalyzerEssentiaKey`
**継承元：** `Analyzer`
**追加場所：** `src/analyzer/essentiaanalyzer.h/.cpp`（新規）

```
AnalyzerManager
  ├── AnalyzerQueenMaryBeats（既存）        ← フォールバック
  ├── AnalyzerEssentiaBPM（新規）           ← BeatTrackerMultiFeature 使用
  │     └── フレキシブルBPMグリッド対応      ← 変動テンポを複数ポイントで記録
  ├── AnalyzerQueenMaryKey（既存）
  └── AnalyzerEssentiaKey（新規）           ← 設定で切り替え可能
```

**設計ポイント：**
- `AnalyzerManager` の解析エンジン選択を設定画面の `UserSettingsPointer` で切り替える
- Essentia の C++ API を直接呼び出す（subprocess ではない）
- フレキシブル BPM グリッドは `AnalyzerEssentiaBPM` の出力から複数 BPM ポイントを生成し `BeatsFactory` へ渡す

---

### 4. ステム分離（Demucs v4 ONNX）

**追加クラス：** `StemSeparator`, `StemCache`, `StemController`
**追加場所：** `src/stem/`（新規ディレクトリ）

```
StemSeparator（新規・シングルトン）
  ├── OrtSession（ONNX Runtime）      ← Demucs モデルのロード・推論
  ├── StemCache                        ← 分離済みステムファイルのキャッシュ管理
  ├── StemWorkerThread                 ← 推論を専用スレッドで実行（オーディオスレッド非干渉）
  └── StemController                   ← デッキとステムのミュート状態管理

PlayerManager
  └── BaseTrackPlayer
        └── StemController 参照        ← ステムミュート命令を受け取る
```

**パッド連携：**
```
ControllerEngine（JS マッピング）
  └── StemMode パッド割り当て
        ├── PAD1 押下 → engine.setValue("[Channel1]", "stem_drums_mute", toggle)
        ├── PAD2 押下 → engine.setValue("[Channel1]", "stem_bass_mute", toggle)
        ├── PAD3 押下 → engine.setValue("[Channel1]", "stem_vocals_mute", toggle)
        └── PAD4 押下 → engine.setValue("[Channel1]", "stem_other_mute", toggle)
```

**設計ポイント：**
- ONNX Runtime の推論スレッドは Qt の `QThreadPool` で管理
- ステムキャッシュは `%APPDATA%\RAIDJ\stem_cache\` に保存
- モデルファイルは `%APPDATA%\RAIDJ\models\demucs_v4.onnx` に配置

---

### 5. エフェクト強化

**追加クラス：** 各エフェクトクラス（`RaidjFlangerEffect` 等）
**継承元：** `EffectProcessor`
**追加場所：** `src/effects/backends/builtin/raidj/`（新規ディレクトリ）

**DSP 実装方針：Mixxx 既存の Signal Processing コードを流用**
- `src/engine/filters/` の既存フィルター実装を参考・流用
- 追加依存ライブラリなし。Mixxx のコードスタイルに統一

```
EffectsManager
  └── BuiltInBackend
        ├── 既存エフェクト群（流用・参考）
        └── RAIDJ 追加エフェクト群（新規・既存 DSP コード流用）
              ├── RaidjFlangerEffect   ← EngineFilterBiquad 等を流用
              ├── RaidjPhaserEffect
              ├── RaidjBitcrusherEffect
              ├── RaidjReverbEffect
              └── RaidjBpmSyncDelay    ← BPM 同期ディレイ（ControlProxy で BPM 取得）
```

**VST3 対応（フェーズ 4）：VST3 SDK 直接使用**
```
EffectsManager
  └── VST3Backend（新規・フェーズ4）
        └── Vst3PluginLoader           ← Steinberg VST3 SDK（GPL v3・個人利用無償）
```

---

### 6. VJ エンジン

**追加クラス：** `VJEngine`, `VJRenderer`, `VJSceneManager`, `NDIOutput`
**追加場所：** `src/vj/`（新規ディレクトリ）

```
VJEngine（新規・シングルトン）
  ├── VJAudioAnalyzer               ← FFT エネルギー・ドロップ/ブレイク検出
  │     ├── Mixxx 波形表示用 FFT データを ControlProxy で購読（追加計算コストなし）
  │     ├── ドロップ：低域(20〜250Hz)エネルギー > 移動平均 × 2.0
  │     ├── ブレイク：全域 RMS < 移動平均 × 0.3
  │     └── 誤検出防止：検出後 4小節クールダウン（BPMから計算）
  ├── VJSceneManager                ← シーン切り替え・映像素材管理
  │     ├── LocalVideoSource        ← Qt6 QMediaPlayer で指定フォルダからランダム再生
  │     └── YouTubeVideoSource      ← YouTube 楽曲の映像を流用
  ├── VJRenderer（OpenGL）          ← FFmpeg xfade + GL Transitions で合成・描画
  │     ├── FFmpegFilterGraph       ← libavfilter xfade フィルターグラフ
  │     ├── GLTransitionShaders     ← GLSL シェーダー 121 種（映像クロスフェード）
  │     └── VJFallbackShader        ← 映像素材なし時のビジュアライザー
  │           └── ShaderToy互換 GLSL（BPM/FFTエネルギーを uniform で渡す）
  ├── VJOutputWindow                ← 外部ディスプレイ / プロジェクター向け出力
  └── NDIOutput                     ← NDI SDK で映像＋音声を送出
        └── OBSWebSocketClient      ← Qt6 QWebSocket で自前実装
                                       OBS WebSocket v5（JSON over WebSocket）
                                       曲名/BPM/Key をテキストソースへ自動反映
```

**イベント購読の設計：**
```
既存の ControlObject（CO）を購読して自動連動：
  [Master] crossfader           → 映像クロスフェード率
  [Channel1/2] beat_active      → ビートフラッシュ
  [Channel1/2] hotcue_*_activate → エフェクトトリガー
  [Channel1/2] loop_enabled     → ループエフェクト
  [Master] bpm                  → アニメーション速度
```

---

### 7. バックアップシステム

**追加クラス：** `BackupManager`, `GoogleDriveClient`
**追加場所：** `src/backup/`（新規ディレクトリ）

```
BackupManager（新規）
  ├── GoogleDriveClient             ← QtNetwork で Google Drive REST API v3 を直叩き
  │     │                             追加依存なし（QNetworkAccessManager 使用）
  │     └── TokenStore              ← Qt Keychain（Windows Credential Manager）でトークン管理
  ├── BackupScheduler               ← 起動時/終了時/定期の自動実行
  ├── DiffTracker                   ← 変更ファイルの追跡（差分バックアップ）
  └── RestoreManager                ← Google Drive からのリストア
```

---

### 8. セトリ管理

**追加クラス：** `SetlistManager`, `SetlistExporter`
**追加場所：** `src/setlist/`（新規ディレクトリ）

```
SetlistManager（新規）
  ├── AutoSetlist                   ← 再生イベントを購読して自動記録
  │     └── PlayerManager の trackLoaded シグナルを購読
  ├── ManualSetlist                 ← 手動記録の開始/停止管理
  └── SetlistExporter               ← 各形式への書き出し
        ├── TextExporter
        ├── CsvExporter
        ├── JsonExporter
        ├── ImageExporter           ← QPainter でカード形式の PNG を生成
```

---

## 設定値（mixxx.cfg）の追加キー一覧

```ini
[RAIDJ]
StemEnabled=true
StemEngine=demucs          # demucs / lalalai
LalalaiApiKey=
VJEnabled=false
VJVideoFolder=C:/Videos/VJ
YouTubeCacheDir=%APPDATA%/RAIDJ/youtube_cache
YouTubeCacheMaxGB=10
YouTubeFavoriteDir=%APPDATA%/RAIDJ/youtube_favorites
BackupEnabled=false
BackupSchedule=on_exit      # on_start / on_exit / daily
AnalyzerBPMEngine=essentia  # qm / essentia
AnalyzerKeyEngine=essentia  # keyfinder / essentia
```

---

## コーディング規約（Mixxx に準拠）

- クラス名：`PascalCase`
- メンバ変数：`m_camelCase`
- 定数：`kCamelCase`
- シグナル：`動詞過去形 + 名詞`（例：`trackLoaded`）
- スロット：`on + 動詞 + 名詞`（例：`onTrackLoaded`）
- スレッド境界をまたぐデータ受け渡しは `Qt::QueuedConnection` で必ずキューイング
- オーディオスレッド内では **メモリアロケーション禁止**

---

最終更新：2026-06-04（全未決定事項を反映）
