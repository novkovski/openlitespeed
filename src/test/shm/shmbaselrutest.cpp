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
#ifdef RUN_TEST

#include <shm/lsshmlruhash.h>

#include <stdio.h>
#include <string.h>
#include "test/unittest-cpp/UnitTest++/src/UnitTest++.h"

static const char *g_pShmDirName = LsShmLock::getDefaultShmDir();
static const char *g_pShmName = "SHMLRUTEST";
static const char *g_pHashName = "SHMLRUHASH";

static void doit(LsShm *pShm);

TEST(ls_ShmBaseLru_test)
{
    char shmfilename[255];
    char lockfilename[255];
    LsShm *pShm;
    snprintf(shmfilename, sizeof(shmfilename), "%s/%s.shm",
      g_pShmDirName, g_pShmName);
    snprintf(lockfilename, sizeof(lockfilename), "%s/%s.lock",
      g_pShmDirName, g_pShmName);
    unlink(shmfilename);
    unlink(lockfilename);

    fprintf(stdout, "shmbaselrutest: [%s/%s]\n", g_pShmName, g_pHashName);
    CHECK((pShm = LsShm::open(g_pShmName, 0, g_pShmDirName)) != NULL);
    if (unlink(shmfilename) != 0)
        perror(shmfilename);
    if (unlink(lockfilename) != 0)
        perror(lockfilename);
    if (pShm == NULL)
        return;

    doit(pShm);
}


static void doit(LsShm *pShm)
{
    const char key0[] = "KEY0";
    const char key1[] = "KEY1";
    const char keyX[] = "KEYX5678901234567";
    const char valX[] = "VALX56789012345678901";
    LsShmPool *pGPool;
    LsShmLruHash *pHash;
    LsShmHElem *pTop;
    LsShmHash::iterator iter;
    LsShmHash::iteroffset iterOff0;
    LsShmHash::iteroffset iterOff1;
    LsShmHash::iteroffset iterOffX;
    LsShmOffset_t offTop;
    ls_str_pair_t parms;
    int flags;
    int cnt;
    int num;

    CHECK((pGPool = pShm->getGlobalPool()) != NULL);
    if (pGPool == NULL)
        return;
    CHECK((pHash = pGPool->getNamedLruHash(
      g_pHashName, 0, LsShmHash::hashXXH32, LsShmHash::compBuf)) != NULL);
    if (pHash == NULL)
        return;
    ls_str_unsafeset(&parms.key, (char *)key0, sizeof(key0) - 1);
    ls_str_unsafeset(&parms.value, NULL, 0);
    flags = LSSHM_FLAG_NONE;
    CHECK((iterOff0 = pHash->getIterator(&parms, &flags)) != 0);
    CHECK(flags == LSSHM_FLAG_CREATED);

    ls_str_unsafeset(&parms.key, (char *)key1, sizeof(key1) - 1);
    CHECK((iterOff1 = pHash->findIterator(&parms)) == 0);
    CHECK((iterOff1 = pHash->updateIterator(&parms)) == 0);

    CHECK((iterOff1 = pHash->insertIterator(&parms)) != 0);
    CHECK(pHash->findIterator(&parms) == iterOff1);
    ls_str_unsafeset(&parms.value, (char *)valX, sizeof(valX) - 1);
    CHECK((iterOff1 = pHash->updateIterator(&parms)) != 0);  // may change
    if (iterOff1 != 0)
    {
        iter = pHash->offset2iterator(iterOff1);
        CHECK(iter->getValLen() == sizeof(valX) - 1);
        CHECK(memcmp(iter->getVal(), valX, iter->getValLen()) == 0);
    }
    ls_str_unsafeset(&parms.value, (char *)valX, 5);
    CHECK(pHash->setIterator(&parms) == iterOff1);  // should use same memory
    CHECK(iter->getValLen() == 5);
    
    ls_str_unsafeset(&parms.key, (char *)keyX, sizeof(keyX) - 1);
    flags = LSSHM_FLAG_NONE;
    CHECK((iterOffX = pHash->getIterator(&parms, &flags)) != 0);
    CHECK(flags == LSSHM_FLAG_CREATED);

    CHECK(pHash->check() == SHMLRU_CHECKOK);
    CHECK(pHash->size() == 3);
    CHECK(pHash->getLruTop() == iterOffX);

    ls_str_unsafeset(&parms.key, (char *)key0, sizeof(key0) - 1);
    flags = LSSHM_FLAG_NONE;
    CHECK(pHash->getIterator(&parms, &flags) == iterOff0);
    CHECK(flags == LSSHM_FLAG_NONE);

    CHECK((offTop = pHash->getLruTop()) == iterOff0);
    if ((int)offTop > 0)
    {
        pTop = pHash->offset2iterator(offTop);
        time_t tmval = pTop->getLruLasttime();
        fprintf(stdout, "[%.*s] %s",
               pTop->getKeyLen(), pTop->getKey(), ctime(&tmval));
        num = pHash->size();
        CHECK((cnt = pHash->trim(tmval)) < num);
        num -= cnt;
        CHECK(pHash->size() == (size_t)num);
        CHECK(pHash->trim(tmval + 1) == num);
        CHECK(pHash->size() == (size_t)0);
        CHECK(pHash->getLruTop() == 0);
    }
    pHash->close();

    return;
}

#endif

