#include "LocaleES.h"
#include <iostream>

#define PACKAGE_LANG "emulationstation2"

#if !defined(WIN32)

#include "SystemConf.h"

#ifndef HAVE_INTL
char* ngettext(char* msgid, char* msgid_plural, unsigned long int n)
{
	if (n != 1)
		return msgid_plural;

	return msgid;
}
#endif

std::string EsLocale::default_LANGUAGE = "";

std::string EsLocale::changeLocale(const std::string& locale) {
	char *clocale = NULL;

	if (locale != "") {
		// this var is looked by gettext in priority (and it is set on most environment, then change it to change the lang)
		if (setenv("LANGUAGE", locale.c_str(), 1) != 0) { /* hum, ok */ }
	}
	else {
		if (setenv("LANGUAGE", default_LANGUAGE.c_str(), 1) != 0) { /* hum, ok */ }
	}

	clocale = setlocale(LC_CTYPE, "");
	clocale = setlocale(LC_MESSAGES, "");

	if (clocale == NULL) {
		return "";
	}

	return clocale;
}

std::string EsLocale::init(std::string locale, std::string path) {
#ifdef HAVE_INTL
	std::string nlocale;
	char* btd;
	char* cs;
	char* envv;

	envv = getenv("LANGUAGE");
	if (envv != NULL) {
		default_LANGUAGE = envv;
	}

	nlocale = changeLocale(locale);

	if (nlocale == "") {
		return locale;
	}

	textdomain(PACKAGE_LANG);
	if ((btd = bindtextdomain(PACKAGE_LANG, path.c_str())) == NULL) {
		return "";
	}

	cs = bind_textdomain_codeset(PACKAGE_LANG, "UTF-8");

	if (cs == NULL) {
		/* outch not enough memory, no real thing to do */
		return locale;
	}

	return nlocale;

#endif
	return "";
}

const bool EsLocale::isRTL()
{
	std::string language = SystemConf::getInstance()->get("system.language");
	return language.find("ar") == 0 || language.find("he") == 0;
}

#else

// For WIN32 avoid using boost or libintl 

#include "resources/ResourceManager.h"
#include "Settings.h"
#include "utils/FileSystemUtil.h"
#include "SystemConf.h"
#include <fstream>

std::map<std::string, std::string> EsLocale::mItems;
std::string EsLocale::mCurrentLanguage = "en_US";
bool EsLocale::mCurrentLanguageLoaded = true; // By default, 'en' is considered loaded

// List of all possible plural forms here
// https://github.com/translate/l10n-guide/blob/master/docs/l10n/pluralforms.rst
// Test rules without spaces & without parenthesis - there are 19 distinct rules

PluralRule rules[] = {
	{ "en", "n!=1", [](int n) { return n != 1 ? 1 : 0; } },
	{ "jp", "0", [](int n) { return 0; } }
};

PluralRule EsLocale::mPluralRule = rules[0];
const std::vector<PluralRule> pluralRules(rules, rules + sizeof(rules) / sizeof(rules[0]));

const std::string EsLocale::getText(const std::string text)
{
	checkLocalisationLoaded();

	auto item = mItems.find(text);
	if (item != mItems.cend())
		return item->second;

	return text;
}

const std::string EsLocale::nGetText(const std::string msgid, const std::string msgid_plural, int n)
{
	if (mCurrentLanguage.empty() || mCurrentLanguage == "en_US") // English default
		return n != 1 ? msgid_plural : msgid;

	if (mPluralRule.rule.empty())
		return n != 1 ? getText(msgid_plural) : getText(msgid);

	checkLocalisationLoaded();

	int pluralId = mPluralRule.evaluate(n);
	if (pluralId == 0)
		return getText(msgid);

	auto item = mItems.find(std::to_string(pluralId) + "@" + msgid_plural);
	if (item != mItems.cend())
		return item->second;

	item = mItems.find(msgid_plural);
	if (item != mItems.cend())
		return item->second;

	return msgid_plural;
}

const bool EsLocale::isRTL()
{
	return mCurrentLanguage.find("ar") == 0 || mCurrentLanguage.find("he") == 0;
}

void EsLocale::checkLocalisationLoaded()
{
	if (mCurrentLanguageLoaded)
	{
		std::string language = SystemConf::getInstance()->get("system.language");
		if (language.empty()) 
			language = "en_US";

		if (language == mCurrentLanguage)
			return;

		mCurrentLanguage = language;
	}

	mCurrentLanguageLoaded = true;

	mPluralRule = rules[0];

	mItems.clear();

	// batocera could be adapted using this path : "/usr/share/locale"
	std::string xmlpath = ResourceManager::getInstance()->getResourcePath(":/locale/" + mCurrentLanguage + "/emulationstation2.po");
	if (!Utils::FileSystem::exists(xmlpath))
		xmlpath = ResourceManager::getInstance()->getResourcePath(":/locale/" + mCurrentLanguage + "/LC_MESSAGES/emulationstation2.po");

	if (!Utils::FileSystem::exists(xmlpath))
		xmlpath = ResourceManager::getInstance()->getResourcePath(":/locale/lang/" + mCurrentLanguage + "/emulationstation2.po");

	if (!Utils::FileSystem::exists(xmlpath))
		xmlpath = ResourceManager::getInstance()->getResourcePath(":/locale/lang/" + mCurrentLanguage + "/LC_MESSAGES/emulationstation2.po");

	if (!Utils::FileSystem::exists(xmlpath))
	{
		auto shortNameDivider = mCurrentLanguage.find("_");
		if (shortNameDivider != std::string::npos)
		{
			auto shortName = mCurrentLanguage.substr(0, shortNameDivider);

			xmlpath = ResourceManager::getInstance()->getResourcePath(":/locale/" + shortName + "/emulationstation2.po");
			if (!Utils::FileSystem::exists(xmlpath))
				xmlpath = ResourceManager::getInstance()->getResourcePath(":/locale/" + shortName + "/LC_MESSAGES/emulationstation2.po");

			if (!Utils::FileSystem::exists(xmlpath))
				xmlpath = ResourceManager::getInstance()->getResourcePath(":/locale/lang/" + shortName + "/emulationstation2.po");

			if (!Utils::FileSystem::exists(xmlpath))
				xmlpath = ResourceManager::getInstance()->getResourcePath(":/locale/lang/" + shortName + "/LC_MESSAGES/emulationstation2.po");
		}
	}

	if (!Utils::FileSystem::exists(xmlpath))
		return;

	std::string	msgid;
	std::string	msgid_plural;

	std::string line;

	std::ifstream file(xmlpath);
	while (std::getline(file, line))
	{
		if (line.find("\"Plural-Forms:") == 0)
		{
			auto start = line.find("plural=");
			if (start != std::string::npos)
			{
				std::string plural;

				auto end = line.find(";", start + 1);
				if (end == std::string::npos)
				{
					plural = line.substr(start + 7, line.size() - start - 7 - 1);

					std::getline(file, line);
					end = line.find(";", start + 1);
					if (end != std::string::npos)
						plural += line.substr(1, end - 1);
				}
				else
					plural = line.substr(start + 7, end - start - 7);

				plural = Utils::String::replace(plural, " ", "");

				if (Utils::String::endsWith(plural, ";"))
					plural = plural.substr(0, plural.size() - 1);

				//	if (Utils::String::startsWith(plural, "(") && Utils::String::endsWith(plural, ")"))
				//		plural = plural.substr(1, plural.size() - 2);			
				plural = Utils::String::replace(plural, "(", "");
				plural = Utils::String::replace(plural, ")", "");

				for (auto iter = pluralRules.cbegin(); iter != pluralRules.cend(); iter++)
				{
					if (plural == iter->rule)
					{
						mPluralRule = *iter;
						break;
					}
				}
			}
		}
		else if (line.find("msgid_plural") == 0)
		{
			auto start = line.find("\"");
			if (start != std::string::npos && !msgid.empty())
			{
				auto end = line.find("\"", start + 1);
				if (end != std::string::npos)
					msgid_plural = line.substr(start + 1, end - start - 1);
			}
		}
		else if (line.find("msgid") == 0)
		{
			msgid = "";
			msgid_plural = "";

			auto start = line.find("\"");
			if (start != std::string::npos)
			{
				auto end = line.find("\"", start + 1);
				if (end != std::string::npos)
					msgid = line.substr(start + 1, end - start - 1);
			}
		}
		else if (line.find("msgstr") == 0)
		{
			std::string	idx;

			if (!msgid_plural.empty())
			{
				auto idxStart = line.find("[");
				if (idxStart != std::string::npos)
				{
					auto idxEnd = line.find("]", idxStart + 1);
					if (idxEnd != std::string::npos)
						idx = line.substr(idxStart + 1, idxEnd - idxStart - 1);
				}
			}

			auto start = line.find("\"");
			if (start != std::string::npos)
			{
				auto end = line.find("\"", start + 1);
				if (end != std::string::npos)
				{
					std::string	msgstr = line.substr(start + 1, end - start - 1);
					if (!msgid.empty() && !msgstr.empty())
						if (idx.empty() || idx == "0")
							mItems[msgid] = msgstr;

					if (!msgid_plural.empty() && !msgstr.empty())
					{
						if (!idx.empty() && idx != "0")
							mItems[idx + "@" + msgid_plural] = msgstr;
						else
							mItems[msgid_plural] = msgstr;
					}
				}
			}
		}
	}
}

#endif
