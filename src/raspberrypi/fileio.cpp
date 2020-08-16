//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2020 GIMONS
//	[ ファイルI/O(RaSCSI用サブセット) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "fileio.h"

//===========================================================================
//
//	ファイルI/O
//
//===========================================================================

#ifndef BAREMETAL
//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
Fileio::Fileio()
{
	// ワーク初期化
	handle = -1;
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
Fileio::~Fileio()
{
	ASSERT(handle == -1);

	// Releaseでの安全策
	Close();
}

//---------------------------------------------------------------------------
//
//	ロード
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Load(const Filepath& path, void *buffer, int size)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle < 0);

	// オープン
	if (!Open(path, ReadOnly)) {
		return FALSE;
	}

	// 読み込み
	if (!Read(buffer, size)) {
		Close();
		return FALSE;
	}

	// クローズ
	Close();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Save(const Filepath& path, void *buffer, int size)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle < 0);

	// オープン
	if (!Open(path, WriteOnly)) {
		return FALSE;
	}

	// 書き込み
	if (!Write(buffer, size)) {
		Close();
		return FALSE;
	}

	// クローズ
	Close();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Open(LPCTSTR fname, OpenMode mode, BOOL directIO)
{
	mode_t omode;

	ASSERT(this);
	ASSERT(fname);
	ASSERT(handle < 0);

	// ヌル文字列からの読み込みは必ず失敗させる
	if (fname[0] == _T('\0')) {
		handle = -1;
		return FALSE;
	}

#ifndef __APPLE__
	// デフォルトモード
	omode = directIO ? O_DIRECT : 0;
#endif // ifndef __APPLE__

	// モード別
	switch (mode) {
		// 読み込みのみ
		case ReadOnly:
			handle = open(fname, O_RDONLY | omode);
			break;

		// 書き込みのみ
		case WriteOnly:
			handle = open(fname, O_CREAT | O_WRONLY | O_TRUNC | omode, 0666);
			break;

		// 読み書き両方
		case ReadWrite:
			// CD-ROMからの読み込みはRWが成功してしまう
			if (access(fname, 0x06) != 0) {
				return FALSE;
			}
			handle = open(fname, O_RDWR | omode);
			break;

		// アペンド
		case Append:
			handle = open(fname, O_CREAT | O_WRONLY | O_APPEND | omode, 0666);
			break;

		// それ以外
		default:
			ASSERT(FALSE);
			break;
	}

	// 結果評価
	if (handle == -1) {
		return FALSE;
	}

	ASSERT(handle >= 0);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Open(LPCTSTR fname, OpenMode mode)
{
	ASSERT(this);

	return Open(fname, mode, FALSE);
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Open(const Filepath& path, OpenMode mode)
{
	ASSERT(this);

	return Open(path.GetPath(), mode);
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::OpenDIO(LPCTSTR fname, OpenMode mode)
{
	ASSERT(this);

	// O_DIRECT付きでオープン
	if (!Open(fname, mode, TRUE)) {
		// 通常モードリトライ(tmpfs等)
		return Open(fname, mode, FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::OpenDIO(const Filepath& path, OpenMode mode)
{
	ASSERT(this);

	return OpenDIO(path.GetPath(), mode);
}

//---------------------------------------------------------------------------
//
//	読み込み
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Read(void *buffer, int size)
{
	int count;

	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle >= 0);

	// 読み込み
	count = read(handle, buffer, size);
	if (count != size) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	書き込み
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Write(const void *buffer, int size)
{
	int count;

	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle >= 0);

	// 書き込み
	count = write(handle, buffer, size);
	if (count != size) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	シーク
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Seek(off64_t offset, BOOL relative)
{
	ASSERT(this);
	ASSERT(handle >= 0);
	ASSERT(offset >= 0);

	// 相対シークならオフセットに現在値を追加
	if (relative) {
		offset += GetFilePos();
	}

	if (lseek(handle, offset, SEEK_SET) != offset) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ファイルサイズ取得
//
//---------------------------------------------------------------------------
off64_t FASTCALL Fileio::GetFileSize()
{
	off64_t cur;
	off64_t end;

	ASSERT(this);
	ASSERT(handle >= 0);

	// ファイル位置を64bitで取得
	cur = GetFilePos();

	// ファイルサイズを64bitで取得
	end = lseek(handle, 0, SEEK_END);

	// 位置を元に戻す
	Seek(cur);

	return end;
}

//---------------------------------------------------------------------------
//
//	ファイル位置取得
//
//---------------------------------------------------------------------------
off64_t FASTCALL Fileio::GetFilePos() const
{
	off64_t pos;

	ASSERT(this);
	ASSERT(handle >= 0);

	// ファイル位置を64bitで取得
	pos = lseek(handle, 0, SEEK_CUR);
	return pos;

}

//---------------------------------------------------------------------------
//
//	クローズ
//
//---------------------------------------------------------------------------
void FASTCALL Fileio::Close()
{
	ASSERT(this);

	if (handle != -1) {
		close(handle);
		handle = -1;
	}
}
#else
//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
Fileio::Fileio()
{
	// ワーク初期化
	handle.obj.fs = 0;
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
Fileio::~Fileio()
{
	ASSERT(!handle.obj.fs);

	// Releaseでの安全策
	Close();
}

//---------------------------------------------------------------------------
//
//	ロード
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Load(const Filepath& path, void *buffer, int size)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(!handle.obj.fs);

	// オープン
	if (!Open(path, ReadOnly)) {
		return FALSE;
	}

	// 読み込み
	if (!Read(buffer, size)) {
		Close();
		return FALSE;
	}

	// クローズ
	Close();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Save(const Filepath& path, void *buffer, int size)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(!handle.obj.fs);

	// オープン
	if (!Open(path, WriteOnly)) {
		return FALSE;
	}

	// 書き込み
	if (!Write(buffer, size)) {
		Close();
		return FALSE;
	}

	// クローズ
	Close();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Open(LPCTSTR fname, OpenMode mode)
{
	FRESULT fr;
	Filepath fpath;
	ASSERT(this);
	ASSERT(fname);
	ASSERT(!handle.obj.fs);

	// ヌル文字列からの読み込みは必ず失敗させる
	if (fname[0] == _T('\0')) {
		return FALSE;
	}

	// モード別
	switch (mode) {
		// 読み込みのみ
		case ReadOnly:
			fr = f_open(&handle, fname, FA_READ);
			break;

		// 書き込みのみ
		case WriteOnly:
			fr = f_open(&handle, fname, FA_CREATE_ALWAYS | FA_WRITE);
			break;

		// 読み書き両方
		case ReadWrite:
			fr = f_open(&handle, fname, FA_READ | FA_WRITE);
			break;

		// アペンド
		case Append:
			fr = f_open(&handle, fname, FA_OPEN_APPEND | FA_WRITE);
			break;

		// それ以外
		default:
			fr = FR_NO_PATH;
			ASSERT(FALSE);
			break;
	}

	// 結果評価
	if (fr != FR_OK) {
		return FALSE;
	}

	// オープン成功
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Open(const Filepath& path, OpenMode mode)
{
	ASSERT(this);
	ASSERT(!handle.obj.fs);

	return Open(path.GetPath(), mode);
}

//---------------------------------------------------------------------------
//
//	読み込み
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Read(void *buffer, int size)
{
	FRESULT fr;
	UINT count;

	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle.obj.fs);

	// 読み込み
	fr = f_read(&handle, buffer, size, &count);
	if (fr != FR_OK || count != (unsigned int)size) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	書き込み
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Write(const void *buffer, int size)
{
	FRESULT fr;
	UINT count;

	ASSERT(this);
	ASSERT(buffer);
	ASSERT(size > 0);
	ASSERT(handle.obj.fs);

	// 書き込み
	fr = f_write(&handle, buffer, size, &count);
	if (fr != FR_OK || count != (unsigned int)size) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	シーク
//
//---------------------------------------------------------------------------
BOOL FASTCALL Fileio::Seek(off64_t offset, BOOL relative)
{
	FRESULT fr;

	ASSERT(this);
	ASSERT(offset >= 0);
	ASSERT(handle.obj.fs);

	// 相対シークならオフセットに現在値を追加
	if (relative) {
		offset += f_tell(&handle);
	}

    fr = f_lseek(&handle, offset);
	if (fr != FR_OK) {
		return FALSE;
	}

	if (f_tell(&handle) != (DWORD)offset) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ファイルサイズ取得
//
//---------------------------------------------------------------------------
off64_t FASTCALL Fileio::GetFileSize()
{
	ASSERT(this);
	ASSERT(handle.obj.fs);

	return f_size(&handle);
}

//---------------------------------------------------------------------------
//
//	ファイル位置取得
//
//---------------------------------------------------------------------------
off64_t FASTCALL Fileio::GetFilePos() const
{
	ASSERT(this);
	ASSERT(handle.obj.fs);

	return f_tell(&handle);
}

//---------------------------------------------------------------------------
//
//	クローズ
//
//---------------------------------------------------------------------------
void FASTCALL Fileio::Close()
{
	ASSERT(this);

	if (handle.obj.fs) {
		f_close(&handle);
	}
}
#endif	//BAREMETAL