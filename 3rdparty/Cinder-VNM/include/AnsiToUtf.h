#pragma once

#if defined( CINDER_MSW )
#include <string>
#include <windows.h>

// Utf16(unicode) is preferred by Windows system
// Utf8 is preferred by Cinder
// Ansi is preferred by native application (notepad/VisualStudio)
static std::wstring AnsiToUtf16( const std::string& str )
{
    int textlen ;
    textlen = MultiByteToWideChar( CP_ACP, 0, str.c_str(),-1, NULL,0 ); 
    wchar_t* buf = (wchar_t *)malloc((textlen+1)*sizeof(wchar_t)); 
    memset(buf,0,(textlen+1)*sizeof(wchar_t)); 
    MultiByteToWideChar(CP_ACP, 0,str.c_str(),-1,(LPWSTR)buf,textlen ); 

    std::wstring result(buf);
    free(buf);
    return result;
}

static std::string Utf16ToAnsi( const std::wstring& str )
{
    int textlen;
    textlen = WideCharToMultiByte( CP_ACP, 0, str.c_str(), -1, NULL, 0, NULL, NULL );
    char* buf =(char *)malloc((textlen+1)*sizeof(char));
    memset( buf, 0, sizeof(char) * ( textlen + 1 ) );
    WideCharToMultiByte( CP_ACP, 0, str.c_str(), -1, buf, textlen, NULL, NULL );

    std::string result(buf);
    free(buf);
    return result;
}

static std::wstring Utf8ToUtf16( const std::string& str )
{
    int textlen ;
    textlen = MultiByteToWideChar( CP_UTF8, 0, str.c_str(),-1, NULL,0 ); 
    wchar_t* buf = (wchar_t *)malloc((textlen+1)*sizeof(wchar_t)); 
    memset(buf,0,(textlen+1)*sizeof(wchar_t)); 
    MultiByteToWideChar(CP_UTF8, 0,str.c_str(),-1,(LPWSTR)buf,textlen ); 

    std::wstring result(buf);
    free(buf);
    return result;
}

static std::string Utf16ToUtf8( const std::wstring& str )
{
    int textlen;
    textlen = WideCharToMultiByte( CP_UTF8, 0, str.c_str(), -1, NULL, 0, NULL, NULL );
    char* buf =(char *)malloc((textlen+1)*sizeof(char));
    memset(buf, 0, sizeof(char) * ( textlen + 1 ) );
    WideCharToMultiByte( CP_UTF8, 0, str.c_str(), -1, buf, textlen, NULL, NULL );

    std::string result(buf);
    free(buf);
    return result;
}

static std::string AnsiToUtf8( const std::string& str )
{
    return Utf16ToUtf8(AnsiToUtf16(str));
}

static std::string Utf8ToAnsi( const std::string& str )
{
    return Utf16ToAnsi(Utf8ToUtf16(str));
}
#endif
