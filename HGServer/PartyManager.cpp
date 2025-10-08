// PartyManager.cpp: implementation of the PartyManager class.
//
//////////////////////////////////////////////////////////////////////

#include "PartyManager.h"

extern char G_cTxt[120];
extern void PutLogList(const char* cMsg);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

PartyManager::PartyManager(class CGame* pGame)
{
	int i;

	for (i = 0; i < DEF_MAXPARTY; i++) {
		m_iMemberNumList[i] = 0;
		m_stMemberNameList[i].m_iPartyID = 0;
		m_stMemberNameList[i].m_iIndex = 0;
		m_stMemberNameList[i].m_dwServerChangeTime = 0;
		ZeroMemory(m_stMemberNameList[i].m_cName, sizeof(m_stMemberNameList[i].m_cName));
	}

	m_pGame = pGame;
	m_dwCheckMemberActTime = timeGetTime();
}

PartyManager::~PartyManager()
{

}

int PartyManager::iCreateNewParty(char* pMasterName)
{
	int i, iPartyID;

	// ??? PartyMaster? ????? ?? 
	for (i = 1; i < DEF_MAXPARTY; i++)
		if ((m_stMemberNameList[i].m_iPartyID != 0) && (strcmp(m_stMemberNameList[i].m_cName, pMasterName) == 0)) return 0;

	iPartyID = 0;
	for (i = 1; i < DEF_MAXPARTY; i++)
		if (m_iMemberNumList[i] == 0) {
			// Party ID? i, ??? ??
			iPartyID = i;
			m_iMemberNumList[iPartyID]++;
			goto CNP_BREAKLOOP;
		}

	return 0;

CNP_BREAKLOOP:;
	// ?? ??? ????.
	for (i = 1; i < DEF_MAXPARTY; i++)
		if (m_stMemberNameList[i].m_iPartyID == 0) {
			m_stMemberNameList[i].m_iPartyID = iPartyID;
			ZeroMemory(m_stMemberNameList[i].m_cName, sizeof(m_stMemberNameList[i].m_cName));
			strcpy(m_stMemberNameList[i].m_cName, pMasterName);
			m_stMemberNameList[i].m_iIndex = 1;

			//testcode
			wsprintf(G_cTxt, "New party(ID:%d Master:%s)", iPartyID, pMasterName);
			PutLogList(G_cTxt);

			return iPartyID;
		}

	return 0;
}

bool PartyManager::bDeleteParty(int iPartyID)
{
	int i;
	bool bFlag;
	char* cp, cData[120];
	DWORD* dwp;
	WORD* wp;

	bFlag = false;
	m_iMemberNumList[iPartyID] = 0;

	for (i = 1; i < DEF_MAXPARTY; i++)
		if (m_stMemberNameList[i].m_iPartyID == iPartyID) {
			m_stMemberNameList[i].m_iPartyID = 0;
			m_stMemberNameList[i].m_iIndex = 0;
			m_stMemberNameList[i].m_dwServerChangeTime = 0;
			ZeroMemory(m_stMemberNameList[i].m_cName, sizeof(m_stMemberNameList[i].m_cName));
			bFlag = true;
		}

	// ?? ?? ???? ??? ????? ????.
	ZeroMemory(cData, sizeof(cData));
	cp = (char*)cData;

	wp = (WORD*)cp;
	*wp = 2; // Code 2: ?? ?? 
	cp += 2;
	wp = (WORD*)cp;
	*wp = iPartyID;
	cp += 2;
	m_pGame->PartyOperationResultHandler(cData); //m_pGateCore->SendMsgToAllGameServers(0, cData, 10, true);

	//testcode
	wsprintf(G_cTxt, "Delete party(ID:%d)", iPartyID);
	PutLogList(G_cTxt);

	return bFlag;
}

bool PartyManager::bAddMember(int iPartyID, char* pMemberName)
{
	int i;


	if (m_iMemberNumList[iPartyID] >= DEF_MAXPARTYMEMBER) return false;

	// ?? ??? ?? ???? ??
	for (i = 1; i < DEF_MAXPARTY; i++)
		if ((m_stMemberNameList[i].m_iPartyID != 0) && (strcmp(m_stMemberNameList[i].m_cName, pMemberName) == 0))
		{
			m_stMemberNameList[i].m_iPartyID = 0;
			m_stMemberNameList[i].m_iIndex = 0;
			m_stMemberNameList[i].m_dwServerChangeTime = 0;
			ZeroMemory(m_stMemberNameList[i].m_cName, sizeof(m_stMemberNameList[i].m_cName));

			m_iMemberNumList[iPartyID]--;
			if (m_iMemberNumList[iPartyID] <= 1) bDeleteParty(iPartyID); // ???? 1?? ??? ?? ??
		}
	//		return false;


	for (i = 1; i < DEF_MAXPARTY; i++)
		if (m_stMemberNameList[i].m_iPartyID == 0) {
			m_stMemberNameList[i].m_iPartyID = iPartyID;
			m_stMemberNameList[i].m_iIndex = 1;
			m_stMemberNameList[i].m_dwServerChangeTime = 0;
			ZeroMemory(m_stMemberNameList[i].m_cName, sizeof(m_stMemberNameList[i].m_cName));
			strcpy(m_stMemberNameList[i].m_cName, pMemberName);
			m_iMemberNumList[iPartyID]++;

			//testcode
			wsprintf(G_cTxt, "Add Member: PartyID:%d Name:%s", iPartyID, pMemberName);
			PutLogList(G_cTxt);
			return true;
		}

	return false;
}

bool PartyManager::bRemoveMember(int iPartyID, char* pMemberName)
{
	int i;

	for (i = 1; i < DEF_MAXPARTY; i++)
		if ((m_stMemberNameList[i].m_iPartyID == iPartyID) && (strcmp(m_stMemberNameList[i].m_cName, pMemberName) == 0)) {

			m_stMemberNameList[i].m_iPartyID = 0;
			m_stMemberNameList[i].m_iIndex = 0;
			m_stMemberNameList[i].m_dwServerChangeTime = 0;
			ZeroMemory(m_stMemberNameList[i].m_cName, sizeof(m_stMemberNameList[i].m_cName));

			m_iMemberNumList[iPartyID]--;
			if (m_iMemberNumList[iPartyID] <= 1) bDeleteParty(iPartyID); // ???? 1?? ??? ?? ?? 

			//testcode
			wsprintf(G_cTxt, "Remove Member: PartyID:%d Name:%s", iPartyID, pMemberName);
			PutLogList(G_cTxt);
			return true;
		}

	return false;
}


bool PartyManager::bCheckPartyMember(int iGSCH, int iPartyID, char* pName)
{
	int i, iRet;
	char* cp, cData[120];
	DWORD* dwp;
	WORD* wp;

	for (i = 1; i < DEF_MAXPARTY; i++)
		if ((m_stMemberNameList[i].m_iPartyID == iPartyID) && (strcmp(m_stMemberNameList[i].m_cName, pName) == 0)) {
			// ?? ?? ?? ??? 
			m_stMemberNameList[i].m_dwServerChangeTime = 0;
			return true;
		}

	// ?? ??? ???. ???? ????.
	ZeroMemory(cData, sizeof(cData));
	cp = (char*)cData;

	wp = (WORD*)cp;
	*wp = 3; // ?? ??? ???? ??
	cp += 2;
	wp = (WORD*)cp;
	*wp = iGSCH;
	cp += 2;
	memcpy(cp, pName, 10);
	cp += 10;
	m_pGame->PartyOperationResultHandler(cData); //iRet = m_pGateCore->m_pClientList[iClientH]->m_pXSock->iSendMsg(cData, 20);

	return false;
}

bool PartyManager::bGetPartyInfo(int iGSCH, char* pName, int iPartyID)
{
	int i, iRet, iTotal;
	char* cp, cData[1024];
	DWORD* dwp;
	WORD* wp, * wpTotal;

	ZeroMemory(cData, sizeof(cData));
	cp = (char*)cData;

	wp = (WORD*)cp;
	*wp = 5; // ?? ???? ??? ??
	cp += 2;
	wp = (WORD*)cp;
	*wp = iGSCH;
	cp += 2;
	memcpy(cp, pName, 10);
	cp += 10;
	wp = (WORD*)cp;
	wpTotal = wp;
	cp += 2;

	iTotal = 0;
	for (i = 1; i < DEF_MAXPARTY; i++)
		if ((m_stMemberNameList[i].m_iPartyID == iPartyID) && (m_stMemberNameList[i].m_iPartyID != 0)) {
			memcpy(cp, m_stMemberNameList[i].m_cName, 10);
			cp += 11;
			iTotal++;
		}

	*wpTotal = iTotal;
	m_pGame->PartyOperationResultHandler(cData); //iRet = m_pGateCore->m_pClientList[iClientH]->m_pXSock->iSendMsg(cData, 20 + iTotal * 11 + 1);

	return true;
}

void PartyManager::GameServerDown()
{
	int i;

	for (i = 0; i < DEF_MAXPARTY; i++)
		if (m_stMemberNameList[i].m_iIndex == 1) {
			//testcode
			wsprintf(G_cTxt, "Removing Party member(%s) by Server down", m_stMemberNameList[i].m_cName);
			PutLogList(G_cTxt);

			m_iMemberNumList[m_stMemberNameList[i].m_iPartyID]--;
			m_stMemberNameList[i].m_iPartyID = 0;
			m_stMemberNameList[i].m_iIndex = 0;
			m_stMemberNameList[i].m_dwServerChangeTime = 0;
			ZeroMemory(m_stMemberNameList[i].m_cName, sizeof(m_stMemberNameList[i].m_cName));
		}
}

void PartyManager::SetServerChangeStatus(char* pName, int iPartyID)
{
	int i;

	for (i = 1; i < DEF_MAXPARTY; i++)
		if ((m_stMemberNameList[i].m_iPartyID == iPartyID) && (strcmp(m_stMemberNameList[i].m_cName, pName) == 0)) {
			m_stMemberNameList[i].m_dwServerChangeTime = timeGetTime();
			return;
		}
}

void PartyManager::CheckMemberActivity()
{
	int i;
	DWORD* dwp, dwTime = timeGetTime();
	char* cp, cData[120];
	WORD* wp;

	if ((dwTime - m_dwCheckMemberActTime) > 1000 * 2) {
		m_dwCheckMemberActTime = dwTime;
	}
	else return;

	for (i = 1; i < DEF_MAXPARTY; i++)
		if ((m_stMemberNameList[i].m_dwServerChangeTime != 0) && ((dwTime - m_stMemberNameList[i].m_dwServerChangeTime) > 1000 * 20)) {
			// ?? ??. 
			ZeroMemory(cData, sizeof(cData));
			cp = (char*)cData;

			wp = (WORD*)cp;
			*wp = 6; // ?? ??? ?? ????.
			cp += 2;
			*cp = 1; // ?? ?? 
			cp++;
			wp = (WORD*)cp;
			*wp = 0;  // ???? 0 
			cp += 2;
			memcpy(cp, m_stMemberNameList[i].m_cName, 10);
			cp += 10;
			wp = (WORD*)cp;
			*wp = (WORD)m_stMemberNameList[i].m_iPartyID;
			cp += 2;
			// ?? ??? ?????? ??? ?? ??? ????.
			m_pGame->PartyOperationResultHandler(cData); //m_pGateCore->SendMsgToAllGameServers(0, cData, 22, true);

			bRemoveMember(m_stMemberNameList[i].m_iPartyID, m_stMemberNameList[i].m_cName);
		}
}