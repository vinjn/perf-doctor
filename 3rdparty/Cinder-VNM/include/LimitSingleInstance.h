#pragma once

#include <windows.h> 

//This code is from Q243953 in case you lose the article and wonder
//where this code came from.
//http://support.microsoft.com/kb/243953/en-us
class LimitSingleInstance
{
protected:
    DWORD  m_dwLastError;
    HANDLE m_hMutex;

public:
    LimitSingleInstance(char *strMutexName)
    {
        //Make sure that you use a name that is unique for this application otherwise
        //two apps may think they are the same if they are using same name for
        //3rd parm to CreateMutex
        m_hMutex = CreateMutexA(NULL, FALSE, strMutexName); //do early
        m_dwLastError = GetLastError(); //save for use later...
    }

    ~LimitSingleInstance() 
    {
        if (m_hMutex)  //Do not forget to close handles.
        {
            CloseHandle(m_hMutex); //Do as late as possible.
            m_hMutex = NULL; //Good habit to be in.
        }
    }

    BOOL IsAnotherInstanceRunning() 
    {
        return (ERROR_ALREADY_EXISTS == m_dwLastError);
    }
};
