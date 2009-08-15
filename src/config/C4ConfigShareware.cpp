/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2003-2004, 2007-2008  Matthes Bender
 * Copyright (c) 2004-2005, 2007  Günther Brammer
 * Copyright (c) 2007  Julian Raschke
 * Copyright (c) 2007  Peter Wortmann
 * Copyright (c) 2001-2009, RedWolf Design GmbH, http://www.clonk.de
 *
 * Portions might be copyrighted by other authors who have contributed
 * to OpenClonk.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * See isc_license.txt for full license and disclaimer.
 *
 * "Clonk" is a registered trademark of Matthes Bender.
 * See clonk_trademark_license.txt for full license.
 */

#include <C4Include.h>
#include <C4ConfigShareware.h>
#include <C4SecurityCertificates.h>
#include <C4Log.h>
#include <C4Gui.h>

#include <StdFile.h>

#ifdef HAVE_IO_H
#include <io.h>
#endif

#ifdef _DEBUG
// this disables registration checking!
#ifdef _WIN32
//#define C4CHECKMEMLEAKS 1
#else
// Do not disable registration checking
#endif
#endif

#include <openssl/md5.h>

#ifndef C4CHECKMEMLEAKS
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/pem.h>

// OpenSSL / Key file handling helpers

// Loads a public key from memory. The parameter provided must point to a certificate in memory.
// If generated by certscramble.pl, the certificate needs to be de-base64ed and de-XORed first.
// The loaded public key has to be cleared using clearPublicKey after use.

EVP_PKEY* loadPublicKey(const char *memKey, bool deBase64 = false, bool deXOR = false, const char *strXOR = 0)
{
	const int maxKeyDataLen = 2048;
	unsigned char keyData[maxKeyDataLen + 1];
	unsigned int keyDataLen;
	memset(keyData, 0, maxKeyDataLen + 1);

  // De-base64 certificate
	if (deBase64)
	{
		// The man page says that the data memKey points to will not be modified by this
		BIO* memBio = BIO_new_mem_buf(const_cast<char *>(memKey), strlen(memKey));
		BIO* b64Bio = BIO_new(BIO_f_base64());
		BIO* bio = BIO_push(b64Bio, memBio);
		BIO_flush(bio);
		keyDataLen = BIO_read(bio, keyData, maxKeyDataLen * 2);
		BIO_free_all(bio);
	}
	else
	{
		keyDataLen = strlen(memKey);
		memcpy(keyData, memKey, keyDataLen);
	}

  // De-XOR certificate
	if (deXOR)
	{
		int xorStrLen = strlen(strXOR);
		for (unsigned int i = 0; i < keyDataLen; i++)
			keyData[i] = keyData[i] ^ strXOR[i % xorStrLen];
	}

	// Load public key from certificate
	BIO* mbio = BIO_new_mem_buf(keyData, -1);
	X509* cert = PEM_read_bio_X509(mbio, NULL, NULL, NULL);
	if (!cert) return 0;
	BIO_free(mbio);
	EVP_PKEY* pubKey = X509_get_pubkey(cert);
	X509_free(cert);

	// Return public key
	return pubKey;
}

void clearPublicKey(EVP_PKEY* pubKey)
{
   EVP_PKEY_free(pubKey);
}

// Verifies the specified block of data using the public key provided.
// The block of data is assumed to include a base64-ed signature separated
// from data by a double-linefeed delimier.

int verifyData(char *data, unsigned int dataLen, EVP_PKEY* pubKey)
{
	// Invalid parameters
	if (!data || !dataLen || !pubKey)
		return -1;

	// Find delimiter in data
	char* pDelimiter = 0;
	for (char* pBack = data + dataLen - 4; pBack >= data; pBack--)
		if (memcmp(pBack, "\r\n\r\n", 4) == 0)
		{
			pDelimiter = pBack;
			break;
		}
	if (!pDelimiter) return -1; //return error("No delimiter found in data to verify.");
	unsigned int dataOnlyLen = pDelimiter - data;

	// De-base64 signature on registration key
	unsigned char* b64Sig = (unsigned char*) pDelimiter + 4;
	unsigned int b64SigLen = dataLen - dataOnlyLen - 4;
	BIO* memBio = BIO_new_mem_buf(b64Sig, b64SigLen);
	BIO* b64Bio = BIO_new(BIO_f_base64());
	BIO* bio = BIO_push(b64Bio, memBio);
	BIO_flush(bio);
	const int MAXSIGLEN = 1024;
	unsigned char sig[MAXSIGLEN] = "";
	unsigned int sigLen = BIO_read(bio, sig, MAXSIGLEN * 2);
	BIO_free_all(bio);

	// Verify signature
	EVP_MD_CTX digest;
	EVP_VerifyInit(&digest, EVP_sha1());
	EVP_VerifyUpdate(&digest, data, dataOnlyLen);
	int err = EVP_VerifyFinal(&digest, sig, sigLen, pubKey);
	if (err != 1)
	return -1;
	EVP_MD_CTX_cleanup(&digest);

	// Data is valid
	return 0;
}

// Windows clipboard handling

bool getClipboardText(char *pBuffer, int iBufferSize)
{
#ifdef _WIN32
	if (!IsClipboardFormatAvailable(CF_TEXT)) return false;
	if (!OpenClipboard(NULL)) return false;
	HANDLE hData = GetClipboardData(CF_TEXT);
	LPVOID pData = GlobalLock(hData);
	strncpy(pBuffer, (const char*) pData, iBufferSize); pBuffer[iBufferSize] = 0;
	GlobalUnlock(pData);
	CloseClipboard();
	return true;
#else
	return false;
#endif
}
#endif // C4CHECKMEMLEAKS

// Old-style string scrambling

#define ScrambleDefaultKey "_D.lp/8f3_ 3 ] =h16%2"

const char ScrambleMark = '�',
					 ScrambleRangeLow = '0',
					 ScrambleRangeHi = 'z';

void ScrambleString(char *szString, int iLen, const char *szKey)
	{
	if (!szString || !szKey) return;
	char *cpString = szString;
	const char *cpKey = szKey;
	int iChar,iEscSequence=0;
	if (iLen<0) iLen=SLen(szString);
	while (iLen>0)
		{
		// Detect character escape sequences
		if (*cpString=='\\') iEscSequence=2;
		// Process character
		if (!iEscSequence)
			{
			// Scramble range
			if (Inside(*cpString,ScrambleRangeLow,ScrambleRangeHi))
				{
				// Shift character
				iChar= ((int) *cpString) + (BoundBy(*cpKey,ScrambleRangeLow,ScrambleRangeHi)-ScrambleRangeLow);
				if (iChar>ScrambleRangeHi) iChar-=(ScrambleRangeHi-ScrambleRangeLow+1);
				*cpString = (char) iChar;
				// Advance key
				cpKey++; if (!(*cpKey)) cpKey=szKey;
				}
			// Postconvert backslash
			if (*cpString=='\\') *cpString='�';
			}
		// Advance
		cpString++; iLen--; if (iEscSequence) iEscSequence--;
		}
	}

void UnscrambleString(char *szString/*, int iLen, const char *szKey*/)
	{
	int iLen=-1;
	const char *szKey = ScrambleDefaultKey;

	if (!szString || !szKey) return;
	char *cpString = szString;
	const char *cpKey = szKey;
	int iChar,iEscSequence=0;
	if (iLen<0) iLen=SLen(szString);
	while (iLen>0)
		{
		// Detect character escape sequences
		if (*cpString=='\\') iEscSequence=2;
		// Process character
		if (!iEscSequence)
			{
			// Preconvert backslash
			if (*cpString=='�')	*cpString='\\';
			// Scramble range
			if (Inside(*cpString,ScrambleRangeLow,ScrambleRangeHi))
				{
				// Unshift character
				iChar= ((int) *cpString) - (BoundBy(*cpKey,ScrambleRangeLow,ScrambleRangeHi)-ScrambleRangeLow);
				if (iChar<ScrambleRangeLow) iChar+=(ScrambleRangeHi-ScrambleRangeLow+1);
				*cpString = (char) iChar;
				// Advance key
				cpKey++; if (!(*cpKey)) cpKey=szKey;
				}
			}
		cpString++; iLen--; if (iEscSequence) iEscSequence--;
		}
	}

#define IDS_SEC_FREEFOLDERMAKER "�6ydHdln zhWijn"
#define IDS_SEC_FREEFOLDERS     "�8>t`giiW.fcf>ECs;.P5l;OS>sV.X4nqNIyeoCrq.P5l;YS;lUh.c<Q>6afeU.cAS<SenIys.TtfC8dTs.f4H;QfogmkG.w4W0Mq^vMoqs.E4s"

const char *LoadSecStr(const char *szString)
	{
	static char szBuf[1024+1];
	SCopy(szString,szBuf);
	UnscrambleString(szBuf);
	return szBuf+1;
	}

C4ConfigShareware::C4ConfigShareware()
	{
	KeyFile[0] = 0;
	InvalidKeyFile[0] = 0;
	}

C4ConfigShareware::~C4ConfigShareware()
	{

	}

void C4ConfigShareware::Default()
	{
  ZeroMem(this, sizeof (C4ConfigShareware));
	C4Config::Default();
	}

bool C4ConfigShareware::Load(bool forceWorkingDirectory, const char *szCustomFile)
	{
	// Load standard config
	if (!C4Config::Load(forceWorkingDirectory, szCustomFile)) return false;
	// Load registration
	LoadRegistration();
	// Done
	return true;
	}

bool C4ConfigShareware::Save()
	{
	// Save standard config
	if (!C4Config::Save()) return false;
	// Done
	return true;
	}

bool C4ConfigShareware::Registered()
	{
	return RegistrationValid;
	}

bool C4ConfigShareware::LoadRegistration()
	{
	// Reset error message(s)
	RegistrationError.Clear();

	// First look in configured KeyPath
	if (Security.KeyPath[0])
		{
		char searchPath[_MAX_PATH] = "";
		SCopy(GetKeyPath(), searchPath);
		for (DirectoryIterator i(searchPath); *i; ++i)
			if (WildcardMatch("*.c4k", *i))
				if (LoadRegistration(*i))
					return true;
				else
					SCopy(*i, InvalidKeyFile, CFG_MaxString);
		}

	// Then look in ExePath
	for (DirectoryIterator i(General.ExePath); *i; ++i)
		if (WildcardMatch("*.c4k", *i))
			if (LoadRegistration(*i))
				return true;
			else
				SCopy(*i, InvalidKeyFile, CFG_MaxString);

	// No key file found
	return HandleError("No valid key file found.");
	}

bool C4ConfigShareware::LoadRegistration(const char *keyFile)
	{

#ifdef C4CHECKMEMLEAKS
	// Set registered flag
	RegistrationValid = true;
	SCopy("Debug User", General.Name);
	return true;
#else

	// Clear registered flag, general name, and key file names
	RegistrationValid = false;
	General.Name[0] = 0;
	KeyFile[0] = 0;
	InvalidKeyFile[0] = 0;

	// Load registration key
	char* delim = 0;
	int regKeyLen = 0;
	int dataLen = 0;
	FILE* fh = fopen(keyFile, "rb");
	if (!fh)
		return HandleError("Cannot open key file.");
	regKeyLen = fread(RegData, 1, MaxRegDataLen, fh);
	fclose(fh);
	delim = strstr(RegData, "\r\n\r\n");
	if (!delim)
		return HandleError("Invalid key data (no delimiter).");
	dataLen = delim - RegData;

	// Load public key from memory
	EVP_PKEY* pubKey = loadPublicKey(Cert_Reg_XOR_Base64, true, true, XOR_Cert_Reg);
	if (!pubKey)
		return HandleError("Internal error on public key.");

	// Verify registration data
	if (verifyData(RegData, regKeyLen, pubKey) != 0)
	{
		clearPublicKey(pubKey);
		return HandleError("Registration key not valid.");
	}

	// Clear public key
	clearPublicKey(pubKey);

	// Cuid blacklist (this will ban a certain player-id forever)
	int i;
	const char *BlackCuid[] = { "16206436", "36689615", "15315029", "13109693", "17584580", "14718309",
															"10851931", "13295768", "15786864", "13769103",
															NULL };
	for (i = 0; BlackCuid[i]; i++)
		if (SEqual2(SSearch(RegData, "Cuid="), BlackCuid[i]))
			return HandleError("Invalid User-Id.");

	// Cuid-Webcode blacklist (this will only ban a key with a given cuid-webcode combination, allowing to create a new key without assigning a new cuid)
	const char *BlackCuidWebCode[] = { "16826502", "398ED5F7",
																		 NULL, NULL };
	for (i = 0; BlackCuidWebCode[i]; i += 2)
		if (SEqual2(SSearch(RegData, "Cuid="), BlackCuidWebCode[i]) && SEqual2(SSearch(RegData, "WebCode="), BlackCuidWebCode[i + 1]))
			return HandleError("Invalid User-Id/WebCode.");

	// Now truncate the signature from the registration key
	*delim = 0;

	// Date check: Clonk Rage allows valid c4k keys which are newer than 2006-04-01
	int iYear, iMonth, iDay;
	sscanf(GetRegistrationData("Date"), "%d-%d-%d", &iYear, &iMonth, &iDay);
	if ((iYear < 2006) || ((iYear == 2006) && (iMonth < 4)))
		return HandleError("Registration key is too old for this version.");

	// Set registered flag
	RegistrationValid = true;
	// Store valid keyfile name
	SCopy(keyFile, KeyFile);
	// Overwrite general config name (will be empty if not registered)
	SAppend(GetRegistrationData("FirstName"), General.Name, CFG_MaxString);
	SAppend(" ", General.Name, CFG_MaxString);
	SAppend(GetRegistrationData("LastName"), General.Name, CFG_MaxString);

	// Success
	return true;
#endif
}

bool C4ConfigShareware::HandleError(const char *strMessage)
{
	RegData[0] = 0;
	Log(strMessage);
	if (RegistrationError.getLength()) RegistrationError.Append(" ");
	RegistrationError.Append(strMessage);
	return false;
}

const char* C4ConfigShareware::GetRegistrationError()
{
	return RegistrationError.getData();
}

const char* C4ConfigShareware::GetKeyFilename()
{
	return KeyFile;
}

const char* C4ConfigShareware::GetInvalidKeyFilename()
{
	return InvalidKeyFile;
}

const char* C4ConfigShareware::GetKeyPath()
{
	// Returns the configured key path with environment variables expanded and terminating slash
	static char strKeyPath[_MAX_PATH + 1];
	if (!Security.KeyPath[0]) return "";
	SCopy(Security.KeyPath, strKeyPath);
	ExpandEnvironmentVariables(strKeyPath, _MAX_PATH);
	AppendBackslash(strKeyPath);
	return strKeyPath;
}

StdStrBuf C4ConfigShareware::GetKeyMD5()
{
	if(!Registered()) return StdStrBuf();
	// Calculate MD5 of full key data
	BYTE Digest[MD5_DIGEST_LENGTH];
	MD5(reinterpret_cast<BYTE *>(RegData), SLen(RegData), Digest);
	// Convert to hex
	StdStrBuf HexDigest;
	for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
		HexDigest.AppendFormat("%02x", Digest[i]);
	// Done
	return HexDigest;
}

const char* C4ConfigShareware::GetRegistrationData(const char *strField)
{
	// Notice: GetRegistrationData() is used by LoadRegistration even if the registered flag is not yet set.
	static char strReturnValue[CFG_MaxString + 1];
	// Find field in registration key
	char strFieldMask[128 + 1 + 1];
	strncpy(strFieldMask, strField, 128); strFieldMask[128] = 0;
	strcat(strFieldMask, "=");
	const char *pKeyField = strstr(RegData, strFieldMask);
	// Field not found, key might be empty
	if (!pKeyField) return "";
	// Advance to value
	pKeyField += strlen(strFieldMask);
	// Get field value
	int iValueLen = 256;
	const char *pFieldEnd = strstr(pKeyField, "\x0d");
	if (pFieldEnd) iValueLen = pFieldEnd - pKeyField;
	iValueLen = Min(iValueLen, CFG_MaxString);
	strncpy(strReturnValue, pKeyField, iValueLen); strReturnValue[iValueLen] = 0;
	return strReturnValue;
}

void C4ConfigShareware::ClearRegistrationError()
{
	// Reset error message(s)
	RegistrationError.Clear();
}

bool C4ConfigShareware::IsConfidentialData(const char *szInput)
	{
	// safety
	if (!szInput) return false;
	// unreg users don't have confidential data
	if (!Config.Registered()) return false;
	// shouldn't send the webcode!
	const char *szWebCode = GetRegistrationData("WebCode");
	if (szWebCode && *szWebCode) if (SSearchNoCase(szInput, szWebCode))
		{
/*		if (fShowWarningMessage && ::pGUI)
			::pGUI->ShowErrorMessage(LoadResStr("IDS_ERR_WARNINGYOUWERETRYINGTOSEN"));*/
		return true;
		}
	// all OK
	return false;
	}

C4ConfigShareware Config;
