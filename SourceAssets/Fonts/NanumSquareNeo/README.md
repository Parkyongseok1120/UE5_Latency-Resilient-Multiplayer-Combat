# 나눔스퀘어 네오

- 원본: [네이버 나눔스퀘어 네오](https://campaign.naver.com/nanumsquare_neo/)
- 라이선스: [네이버 공식 안내](https://help.naver.com/service/30016/contents/18088?osType=PC&lang=ko), SIL Open Font License 1.1. 전문은 [OFL.txt](OFL.txt)에 있다.
- `NanumSquareNeo-Regular.ttf`: 공식 배포본 `NanumSquareNeo-bRg.ttf`와 동일한 일반체.
- `NanumSquareNeo-Bold.ttf`: 공식 배포본 `NanumSquareNeo-cBd.ttf`와 동일한 굵은체.

폰트 내부 이름과 데이터는 변경하지 않았다. 원본 TTF를 `/Game/UI/Fonts/FF_NanumSquareNeo_Regular`와 `FF_NanumSquareNeo_Bold`에 Inline 방식으로 포함하므로 실행 PC의 설치 폰트에 의존하지 않는다. `ACombatHUD`는 이 두 FontFace를 참조하는 런타임 Canvas 폰트를 생성한다. 라이선스는 패키징 시 `Licenses/NanumSquareNeo-OFL.txt`로 동봉하도록 RuntimeDependencies에 등록한다.

FontFace 에셋을 다시 만들 때는 Slate가 초기화된 에디터의 Python 세션에서 `Tools/CreateCombatHUDFonts.py`를 실행한다. 일반 Python 또는 Slate가 없는 PythonScript commandlet에서 실행하지 않는다.
