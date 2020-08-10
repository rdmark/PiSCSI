//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ ログ ]
//
//---------------------------------------------------------------------------

#if !defined(log_h)
#define log_h

#ifdef BAREMETAL
#define FPRT(fp, ...) printf( __VA_ARGS__ )
#else
#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )
#endif	// BAREMETAL


//===========================================================================
//
//	ログ
//
//===========================================================================
class Log
{
public:
	enum loglevel {
		Detail,							// 詳細レベル
		Normal,							// 通常レベル
		Warning,						// 警告レベル
		Debug							// デバッグレベル
	};
};

#endif	// log_h
