/*****************************************************************************
*    Open LiteSpeed is an open source HTTP server.                           *
*    Copyright (C) 2013 - 2015  LiteSpeed Technologies, Inc.                 *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation, either version 3 of the License, or       *
*    (at your option) any later version.                                     *
*                                                                            *
*    This program is distributed in the hope that it will be useful,         *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
*    GNU General Public License for more details.                            *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see http://www.gnu.org/licenses/.      *
*****************************************************************************/
#include "cachestore.h"
#include "cachehash.h"
#include "cacheentry.h"
#include <util/datetime.h>

CacheStore::CacheStore()
    : HashStringMap<CacheEntry * >(29,
                                   CacheHash::to_ghash_key,
                                   CacheHash::compare)

{
}


CacheStore::~CacheStore()
{
    m_dirtyList.releaseObjects();
}


int CacheStore::stale(CacheEntry *pEntry)
{
    pEntry->setStale(1);
    if (renameDiskEntry(pEntry, NULL, NULL, ".S", 1) == -1)
    {
        iterator iter = find(pEntry->getHashKey().getKey());
        if (iter != end())
            dispose(iter, 0);
    }
    return 0;
}


int CacheStore::dispose(CacheStore::iterator iter, int isRemovePermEntry)
{
    CacheEntry *pEntry = iter.second();
    erase(iter);
    if (isRemovePermEntry)
        removePermEntry(pEntry);
    if (pEntry->getRef() <= 0)
        delete pEntry;
    else
        m_dirtyList.push_back(pEntry);
    return 0;
}


int CacheStore::purge(CacheEntry  *pEntry)
{
    iterator iter = find(pEntry->getHashKey().getKey());
    if (iter != end())
    {
        dispose(iter, 1);
        return 1;
    }
    return 0;
}


int CacheStore::refresh(CacheEntry  *pEntry)
{
    return stale(pEntry);
    /*
    iterator iter = find( pEntry->getHashKey().getKey() );
    if ( iter != end() )
    {
        iter.second()->setStale( 1 );
        return 1;
    }
    return 0;
    */
}


void CacheStore::houseKeeping()
{
    CacheEntry *pEntry;
    iterator iterEnd = end();
    iterator iterNext;
    for (iterator iter = begin(); iter != iterEnd; iter = iterNext)
    {
        pEntry = (CacheEntry *)iter.second();
        iterNext = GHash::next(iter);
        if (pEntry->getRef() == 0)
        {
            if (DateTime_s_curTime > pEntry->getExpireTime() + pEntry->getMaxStale())
            {
                dispose(iter, 1);
                continue;
            }
            else
            {
                int idle = DateTime_s_curTime - pEntry->getLastAccess();
                if (idle > 120)
                {
                    erase(iter);
                    delete pEntry;
                    continue;
                }
                if (idle > 10)
                    pEntry->releaseTmpResource();
            }
        }
        //else if (DateTime_s_curTime - pEntry->getLastAccess() > 300 )
        //{
        //    LOG_INFO(( "Idle Cache, fd: %d, ref: %d, force release.",
        //                pEntry->getFdStore(), pEntry->getRef() ));
        //    erase( iter );
        //    delete pEntry;
        //}
    }
    for (TPointerList< CacheEntry >::iterator it = m_dirtyList.begin();
            it != m_dirtyList.end();)
    {
        if ((*it)->getRef() == 0)
        {
            delete *it;
            it = m_dirtyList.erase(it);
        }
        //else if (DateTime_s_curTime - (*it)->getLastAccess() > 300 )
        //{
        //    LOG_INFO(( "Unreleased Cache in dirty list, fd: %d, ref: %d, force release.",
        //                (*it)->getFdStore(), (*it)->getRef() ));
        //    delete *it;
        //    it = m_dirtyList.erase( it );
        //}
        else
            ++it;
    }
}

