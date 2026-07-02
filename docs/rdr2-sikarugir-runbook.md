# RDR2 on Sikarugir — 실행 런북 (Steam판, M4)

Mac mini M4에서 **당신이** 복붙 실행하는 런북. (AGENTS.md 골든룰 #3: 프리픽스를
건드리는 명령은 사용자가 실행. Claude는 작성/설명만.)

> 병목 재정리는 [`2026-07-02-rdr2-blocker-reconciliation.md`](2026-07-02-rdr2-blocker-reconciliation.md) 참고.
> 현 상태: ⑤(CEF int3)까지 도달. **최고 레버는 STEP 3의 "엔진 버전 정면돌파".**
> 아래 플레이스홀더는 실제 경로로 치환:
> - `<WRAP>` = Sikarugir 래퍼 `.app` 경로 (예: `~/Applications/RDR2.app`)
> - `<PREFIX>` = 프리픽스 루트 (래퍼 구조 확인 후 기입, 보통 `<WRAP>/Contents/SharedSupport/prefix` 또는 `.../Resources`)
> - `<user>` = 프리픽스 내 윈도우 유저명 (`ls <PREFIX>/drive_c/users`)
> - `<CX_BOTTLE>` = `~/Library/Application Support/CrossOver/Bottles/<보틀명>`

---

## STEP -1 — 머신/버전 스냅샷 (프로파일 DB 기록용)

```bash
sw_vers
sysctl -n machdep.cpu.brand_string
echo "$(( $(sysctl -n hw.memsize)/1024/1024/1024 )) GB"
# 게임 테스트 전 OpenClaw 에이전트 스택 내릴 것 (16GB 램 경합)
```

---

## STEP 0 — 폰트/로케일 (현 블로커 아님, 게임 UI·한국어용으로만)

> ⑤까지 도달했으므로 tofu는 이미 지남. 게임 본체/한국어 렌더용으로만 유지.

```bash
# A. 다이얼로그 영어 강제 — Sikarugir Configure > Advanced > Environment Variables 에 주입
#    LANG=en_US.UTF-8   LC_ALL=en_US.UTF-8

# B. 폰트 주입 (winetricks GUI 불안정 시 수동 복사)
cp /System/Library/Fonts/Supplemental/*.ttf "<PREFIX>/drive_c/windows/Fonts/" 2>/dev/null
# Noto Sans CJK / 맑은고딕 ttf 도 <PREFIX>/drive_c/windows/Fonts/ 로 복사
# winetricks 경로: corefonts (+ cjkfonts)   ← 내장 rockstar 버브는 절대 쓰지 말 것 (#227)
```

---

## STEP 1 — 서비스 타임아웃 픽스 (RGL 무한로딩 고전 원인)

래퍼 내 `regedit` 실행 후 아래 값 생성:

```
HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control
  → 문자열 값(String)  ServicesPipeTimeout = 20000
```

CLI로 하려면 (래퍼 내장 wine 사용):

```bash
WINEPREFIX="<PREFIX>" "<WRAP>/Contents/SharedSupport/wine/bin/wine" \
  reg add "HKLM\System\CurrentControlSet\Control" \
  /v ServicesPipeTimeout /t REG_SZ /d 20000 /f
```

보조: RGL 첫 실패 직후 **강제종료 → 즉시 재실행 2~3회** (비용 0, 성공 사례 다수).

---

## STEP 2 — 런타임 주입

```
winetricks: vcrun2022   (corefonts 는 STEP 0-B 미완 시)
Windows 버전: Windows 10
Sync: ESync ON / MSync OFF     ← MSync 금지 (RDR2 램 누수 + 도시 프레임 급락)
```

①~④ 재발 방지 확인 (엔진 교체/새 프리픽스 시 리셋될 수 있음):

```bash
# ② Error 1002 방지 — Documents 트리 존재 확인/생성
mkdir -p "<PREFIX>/drive_c/users/<user>/Documents/Rockstar Games/Social Club"
mkdir -p "<PREFIX>/drive_c/users/<user>/Documents/Rockstar Games/Red Dead Redemption 2/Settings"
```

③ CEF init 플래그(스플래시까지 도달용) — Launcher 실행 시:
`--no-sandbox --disable-gpu --in-process-gpu --disable-gpu-compositing`
① D3DMetal: **런처 단계 OFF** (게임 본체만 ON).

---

## STEP 3 — CX 보틀 diff (정답지 역산) ★최고 레버

### 3-0. 엔진/Wine 버전 비교 (가장 먼저 — ⑤는 엔진 델타일 개연성 높음)

```bash
# 현재 Sikarugir 엔진 버전
WINEPREFIX="<PREFIX>" "<WRAP>/Contents/SharedSupport/wine/bin/wine" --version

# CX 보틀의 wine 버전 (CX 자체 바이너리)
"/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine" --version
```

- Sikarugir 엔진이 최신(`WS12WineSikarugir 10.0_6+`)이 아니면 **먼저 최신으로 교체 후 RGL 재기동.**
  CX의 CEF 패치가 유입됐다면 ⑤가 사라질 수 있다 → 프리픽스 diff보다 이게 우선.

### 3-1. 폰트 diff

```bash
diff <(ls "<CX_BOTTLE>/drive_c/windows/Fonts" | sort) \
     <(ls "<PREFIX>/drive_c/windows/Fonts" | sort)
```

### 3-2. 레지스트리 diff (각 프리픽스에서 export 후 비교)

```bash
# CX 쪽 (CX 자체 wine)
WINEPREFIX="<CX_BOTTLE>" \
  "/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine" \
  reg export HKLM /tmp/cx_hklm.reg /y

# Sikarugir 쪽 (래퍼 내장 wine)
WINEPREFIX="<PREFIX>" "<WRAP>/Contents/SharedSupport/wine/bin/wine" \
  reg export HKLM /tmp/sik_hklm.reg /y

diff /tmp/cx_hklm.reg /tmp/sik_hklm.reg | less
# 주목: HKLM\System\CurrentControlSet\Control , HKCU\Software\Wine 하위
```

### 3-3. 설치 런타임 diff

```bash
grep -i "installed" "<CX_BOTTLE>/cxbottle.conf" 2>/dev/null
cat "<PREFIX>/winetricks.log" 2>/dev/null
```

**diff에서 나온 차이를 한 번에 하나씩** Sikarugir 프리픽스에 이식 → 이식 항목마다
RGL 재기동 테스트 (bisect). 무엇이 ⑤를 없앴는지 특정되면 프로파일 DB에 확정 기록.

---

## STEP 4 — 게임 본체 기동

1. RGL 로그인 성공 후, 게임 실행 **전** DX12 강제:
   ```
   ~/Documents/Rockstar Games/Red Dead Redemption 2/Settings/system.xml
     → <API>kSettingAPI_DX12</API>
   ```
   (래퍼 내 상응 경로: `<PREFIX>/drive_c/users/<user>/Documents/Rockstar Games/Red Dead Redemption 2/Settings/system.xml`)
2. 게임 본체 단계에서 **D3DMetal ON**.
3. 첫 실행 셰이더 컴파일로 수 분간 무응답처럼 보임 → 죽은 것 아님, 기다릴 것.
4. 벤치마크 1회: 해상도 / 세팅 / 평균 fps 기록 → 프로파일 DB.
   - 참고: 생드니 메모리 릭(텍스처 버그 시 텍스처 Ultra로 완화), Act1 눈밭 그림자 글리치,
     16GB M4 미디엄 40~52fps 레퍼런스.

---

## 산출물 (성공 시)

- [ ] RGL 로그인 화면 렌더 + 로그인 통과 (⑤ 돌파)
- [ ] RDR2 메인 메뉴 도달 (D3DMetal, DX12)
- [ ] 벤치 1회 기록
- [ ] `profiles/red-dead-redemption-2.yaml` 확정 항목 채우기
