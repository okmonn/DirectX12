#include "Sound.h"
#include <tchar.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "winmm.lib")

// コンストラクタ
Sound::Sound(std::weak_ptr<Window>winAdr, std::weak_ptr<Input>inputAdr)
{
	//ウィンドウクラス参照
	win = winAdr;

	//インプットクラス参照
	input = inputAdr;

	//参照結果の初期化
	result = S_OK;

	//セカンダリーバッファの初期化
	bgm = nullptr;

	//プライマリサウンドバッファの初期化
	buffer = nullptr;

	//ダイレクトサウンド
	sound = nullptr;

	//COMの初期化
	CoInitialize(nullptr);


	//エラーを出力に表示させる
#ifdef _DEBUG
	ID3D12Debug *debug = nullptr;
	result = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
	if (FAILED(result))
		int i = 0;
	debug->EnableDebugLayer();
	debug->Release();
	debug = nullptr;
#endif

	result = CreateDirectSound();
	if (result == S_OK)
	{
		result = CreatePrimaryBuffer();
	}
	//result = LoadWAV(&bgm, "sample/サンプル.wav");
}

// デストラクタ
Sound::~Sound()
{
	if (bgm != nullptr)
	{
		//セカンダリーバッファ
		bgm->Stop();

		bgm->Release();
	}

	//プライマリサウンドバッファ
	if (buffer != nullptr)
	{
		buffer->Release();
	}

	//ダイレクトサウンド
	if (sound != nullptr)
	{
		sound->Release();
	}

	// COMの終了
	CoUninitialize();
}

// 初期処理
HRESULT Sound::CreateDirectSound(void)
{
	//ダイレクトサウンド生成
	result = DirectSoundCreate8(nullptr, &sound, nullptr);
	if (FAILED(result))
	{
		OutputDebugString(_T("ダイレクトサウンドの生成：失敗\n"));

		return result;
	}

	/*協調モードとはWindowsで他のアプリとの整合性を取るためのもので、
	例えば自分のプログラムからフォーカスが移った場合は、サウンドを鳴らさないようにするといった設定*/
	result = sound->SetCooperativeLevel(win.lock()->GetWindowHandle(), DSSCL_EXCLUSIVE | DSSCL_PRIORITY);
	if (FAILED(result))
	{
		OutputDebugString(_T("協調モードのセット：失敗\n"));

		return result;
	}

	return result;
}

// プライマリサウンドバッファの生成
HRESULT Sound::CreatePrimaryBuffer(void)
{
	//バッファ設定用構造体の設定
	DSBUFFERDESC desc;
	SecureZeroMemory(&desc, sizeof(desc));
	desc.dwBufferBytes = 0;
	desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
	desc.dwReserved = 0;
	desc.dwSize = sizeof(DSBUFFERDESC);
	desc.guid3DAlgorithm = GUID_NULL;
	desc.lpwfxFormat = NULL;

	//プライマリサウンドバッファ生成
	result = sound->CreateSoundBuffer(&desc, &buffer, nullptr);
	if (FAILED(result))
	{
		OutputDebugString(_T("プライマリサウンドバッファの生成：失敗\n"));

		return result;
	}

	//フォーマット設定用構造体の設定
	WAVEFORMATEX wav;
	SecureZeroMemory(&wav, sizeof(wav));
	wav.cbSize = sizeof(WAVEFORMATEX);
	wav.nChannels = 2;
	wav.nSamplesPerSec = 44100;
	wav.wBitsPerSample = 16;
	wav.nBlockAlign = wav.nChannels * wav.wBitsPerSample / 8;
	wav.nAvgBytesPerSec = wav.nSamplesPerSec * wav.nBlockAlign;
	wav.wFormatTag = WAVE_FORMAT_PCM;

	//フォーマットのセット
	result = buffer->SetFormat(&wav);
	if (FAILED(result))
	{
		OutputDebugString(_T("プライマリサウンドバッファのフォーマットのセット：失敗\n"));

		return result;
	}

	return result;
}

// サウンドバッファの生成
HRESULT Sound::LoadWAV(LPDIRECTSOUNDBUFFER *wavData, const char *fileName)
{
	//ファイルハンドルの構造体
	HMMIO handle;
	SecureZeroMemory(&handle, sizeof(handle));

	//wavファイルを開く
	handle = mmioOpenA((LPSTR)fileName, NULL, MMIO_ALLOCBUF | MMIO_READ | MMIO_COMPAT);
	if (handle == nullptr)
	{
		OutputDebugString(_T("ファイルを開けませんでした：失敗\n"));

		return S_FALSE;
	}

	//RIFFチャンク情報構造体
	MMCKINFO riff;
	SecureZeroMemory(&riff, sizeof(riff));

	//'W''A''V''E'チャンクのチェック
	result = mmioDescend(handle, &riff, NULL, MMIO_FINDRIFF);
	if (riff.fccType != mmioFOURCC('W', 'A', 'V', 'E'))
	{
		//wavファイルを閉じる
		mmioClose(handle, 0);

		OutputDebugString(_T("WAVEチャンクのチェック：失敗\n"));

		return S_FALSE;
	}

	//fmtチャンク情報構造体
	MMCKINFO fmt;
	SecureZeroMemory(&fmt, sizeof(fmt));

	//'f''m''t'チャンクのチェック
	result = mmioDescend(handle, &fmt, &riff, MMIO_FINDCHUNK);
	if (fmt.ckid != mmioFOURCC('f', 'm', 't', ' '))
	{
		//wavファイルを閉じる
		mmioClose(handle, 0);

		OutputDebugString(_T("fmtチャンクのチェック：失敗\n"));

		return S_FALSE;
	}

	//ヘッダーサイズを取得
	UINT headerSize = fmt.cksize;

	//ヘッダーサイズの修正
	if (headerSize < sizeof(WAVEFORMATEX))
	{
		headerSize = sizeof(WAVEFORMATEX);
	}

	//wavのヘッダー情報
	LPWAVEFORMATEX header;

	//ヘッダーのメモリ確保
	header = (LPWAVEFORMATEX)malloc(headerSize);
	if (header == NULL)
	{
		//wavファイルを閉じる
		mmioClose(handle, 0);

		OutputDebugString(_T("メモリ確保：失敗\n"));

		return S_FALSE;
	}
	SecureZeroMemory(header, headerSize);

	//wavのロード
	result = mmioRead(handle, (char*)header, fmt.cksize);
	if (FAILED(result))
	{
		//メモリ開放
		free(header);

		//wavファイルを閉じる
		mmioClose(handle, 0);

		OutputDebugString(_T("WAVデータのロード：失敗\n"));

		return result;
	}

	//fmtチャンクに戻る
	mmioAscend(handle, &fmt, 0);

	//dataチャンク情報構造体
	MMCKINFO data;
	SecureZeroMemory(&data, sizeof(data));

	while (TRUE)
	{
		//検索
		result = mmioDescend(handle, &data, &riff, 0);
		if (FAILED(result))
		{
			//メモリ開放
			free(header);

			//wavファイルを閉じる
			mmioClose(handle, 0);

			OutputDebugString(_T("dataチャンクのチェック：失敗\n"));

			return result;
		}

		//dataチャンクのとき
		if (data.ckid == mmioStringToFOURCCA("data", 0))
		{
			break;
		}

		//次のチャンクへ
		result = mmioAscend(handle, &data, 0);
	}

	//バッファ設定用構造体の設定
	DSBUFFERDESC desc;
	SecureZeroMemory(&desc, sizeof(desc));
	desc.dwBufferBytes = data.cksize;
	desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_STATIC | DSBCAPS_LOCDEFER;
	desc.dwReserved = 0;
	desc.dwSize = sizeof(DSBUFFERDESC);
	desc.guid3DAlgorithm = DS3DALG_DEFAULT;
	desc.lpwfxFormat = header;

	//バッファ生成
	result = sound->CreateSoundBuffer(&desc, wavData, NULL);
	if (FAILED(result))
	{
		//メモリ開放
		free(header);

		//wavファイルを閉じる
		mmioClose(handle, 0);

		OutputDebugString(_T("セカンダリーバッファの生成：失敗\n"));

		return result;
	}

	//読み取りデータ格納用
	LPVOID mem[2];

	//読み取りサイズ
	DWORD size[2];

	//ロック開始
	result = (*wavData)->Lock(0, data.cksize, &mem[0], &size[0], &mem[1], &size[1], 0);
	if (FAILED(result))
	{
		//メモリ開放
		free(header);

		//wavファイルを閉じる
		mmioClose(handle, 0);

		OutputDebugString(_T("セカンダリーバッファのロック：失敗\n"));

		return result;
	}

	for (UINT i = 0; i < 2; i++)
	{
		//データ書き込み
		mmioRead(handle, (char*)mem[i], size[i]);
	}

	//ロック解除
	(*wavData)->Unlock(mem[0], size[0], mem[1], size[1]);

	//メモリ開放
	free(header);

	//wavファイルを閉じる
	mmioClose(handle, 0);

	return result;
}

// サウンドの再生
HRESULT Sound::Play(LPDIRECTSOUNDBUFFER wavData, DWORD type)
{
	//再生
	result = wavData->Play(0, 0, type);

	return result;
}

// 処理
void Sound::UpData(void)
{
	if (input.lock()->InputKey(DIK_SPACE) == TRUE)
	{
		//ループ再生
		result = Play(bgm, DSBPLAY_LOOPING);
		if (FAILED(result))
		{
			OutputDebugString(_T("再生：失敗"));

			return;
		}
	}
}
