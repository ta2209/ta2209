//
//  Engine.cpp
//  OpenKey
//
//  Created by Tuyen on 1/18/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//
#include <iostream>
#include <algorithm>
#include "Engine.h"
#include <string.h>
#include <list>
#include "Macro.h"

static vector<Uint8> _charKeyCode = {
    KEY_BACKQUOTE, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUALS,
    KEY_LEFT_BRACKET, KEY_RIGHT_BRACKET, KEY_BACK_SLASH,
    KEY_SEMICOLON, KEY_QUOTE, KEY_COMMA, KEY_DOT, KEY_SLASH
};

static vector<Uint8> _breakCode = {
    KEY_ESC, KEY_TAB, KEY_ENTER, KEY_RETURN, KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_UP, KEY_COMMA, KEY_DOT,
    KEY_SLASH, KEY_SEMICOLON, KEY_QUOTE, KEY_BACK_SLASH, KEY_MINUS, KEY_EQUALS, KEY_BACKQUOTE, KEY_TAB
#if _WIN32
	, VK_INSERT, VK_HOME, VK_END, VK_DELETE, VK_PRIOR, VK_NEXT, VK_SNAPSHOT, VK_PRINT, VK_SELECT, VK_HELP,
	VK_EXECUTE, VK_NUMLOCK, VK_SCROLL
#endif
};

static vector<Uint8> _macroBreakCode = {
    KEY_RETURN, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_SEMICOLON, KEY_QUOTE, KEY_BACK_SLASH, KEY_MINUS, KEY_EQUALS
};

static Uint16 ProcessingChar[][11] = {
    {KEY_S, KEY_F, KEY_R, KEY_X, KEY_J, KEY_A, KEY_O, KEY_E, KEY_W, KEY_D, KEY_Z}, //Telex
    {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0}, //VNI
    {KEY_S, KEY_F, KEY_R, KEY_X, KEY_J, KEY_A, KEY_O, KEY_E, KEY_W, KEY_D, KEY_Z}, //Simple Telex 1
    {KEY_S, KEY_F, KEY_R, KEY_X, KEY_J, KEY_A, KEY_O, KEY_E, KEY_W, KEY_D, KEY_Z} //Simple Telex 2
};

#define IS_KEY_Z(key) (ProcessingChar[vInputType][10] == key)
#define IS_KEY_D(key) (ProcessingChar[vInputType][9] == key)
#define IS_KEY_W(key) ((vInputType != vVNI) ? ProcessingChar[vInputType][8] == key : \
                                    (vInputType == vVNI ? (ProcessingChar[vInputType][8] == key || ProcessingChar[vInputType][7] == key) : false))
#define IS_KEY_DOUBLE(key) ((vInputType != vVNI) ? (ProcessingChar[vInputType][5] == key || ProcessingChar[vInputType][6] == key || ProcessingChar[vInputType][7] == key) :\
                                        (vInputType == vVNI ? ProcessingChar[vInputType][6] == key : false))
#define IS_KEY_S(key) (ProcessingChar[vInputType][0] == key)
#define IS_KEY_F(key) (ProcessingChar[vInputType][1] == key)
#define IS_KEY_R(key) (ProcessingChar[vInputType][2] == key)
#define IS_KEY_X(key) (ProcessingChar[vInputType][3] == key)
#define IS_KEY_J(key) (ProcessingChar[vInputType][4] == key)

#define IS_MARK_KEY(keyCode) (((vInputType != vVNI) && (keyCode == KEY_S || keyCode == KEY_F || keyCode == KEY_R || keyCode == KEY_J || keyCode == KEY_X)) || \
                                        (vInputType == vVNI && (keyCode == KEY_1 || keyCode == KEY_2 || keyCode == KEY_3 || keyCode == KEY_5 || keyCode == KEY_4)))
#define IS_BRACKET_KEY(key) (key == KEY_LEFT_BRACKET || key == KEY_RIGHT_BRACKET)

//#define vowelStartIndex vowelStartIndex
//#define vowelEndIndex vowelEndIndex
//#define vowelWillSetMark vowelWillSetMark
//#define HookState.backspaceCount HookState.backspaceCount
//#define HookState.newCharCount HookState.newCharCount
//#define HookState.code HookState.code
//#define HookState.extCode HookState.extCode
//#define HookState.charData HookState.charData
//#define getCharacterCode getCharacterCode
//#define HookState.macroKey HookState.macroKey
//#define HookState.macroData HookState.macroData

//Data to sendback to main program
vKeyHookState HookState;

//private data
/**
 * data structure of each element in TypingWord (Uint64)
 * first 2 byte is character code or key code.
 * bit 16: has caps or not
 * bit 17: has tone ^ or not
 * bit 18: has tone w or not
 * bit 19 - > 23: has mark or not (Sắc, huyền, hỏi, ngã, nặng)
 * bit 24: is standalone key? (w, [, ])
 * bit 25: is character code or keyboard code; 1: character code; 0: keycode
 */
static Uint32 TypingWord[MAX_BUFF];
static Byte _index = 0;
static vector<Uint32> _longWordHelper; //save the word when _index >= MAX_BUFF
static list<vector<Uint32>> _typingStates; //Aug 28th, 2019: typing helper, save long state of Typing word, can go back and modify the word
vector<Uint32> _typingStatesData;

/**
 * Use for restore key if invalid word
 */
static Uint32 KeyStates[MAX_BUFF];
static Byte _stateIndex = 0;

static bool tempDisableKey = false;
static int capsElem;
static int key;
static int markElem;
static bool isCorect = false;
static bool isChanged = false;
static Byte vowelCount = 0;
static Byte vowelStartIndex = 0;
static Byte vowelEndIndex = 0;
static Byte vowelWillSetMark = 0;
static int i, ii, iii;
static int j;
static int k, kk;
static int l;
static bool isRestoredW;
static Uint16 keyForAEO;
static bool isCheckedGrammar;
static bool _isCaps = false;
static int _spaceCount = 0; //add: July 30th, 2019
static bool _hasHandledMacro = false; //for macro flag August 9th, 2019
static Byte _upperCaseStatus = 0; //for Write upper case for the first letter; 2: will upper case
static bool _isCharKeyCode;
static vector<Uint32> _specialChar;
static bool _useSpellCheckingBefore;
static bool _hasHandleQuickConsonant;
static bool _willTempOffEngine = false;

//function prototype
void findAndCalculateVowel(const bool& forGrammar=false);
void insertMark(const Uint32& markMask, const bool& canModifyFlag=true);

static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
wstring utf8ToWideString(const string& str) {
    return converter.from_bytes(str.c_str());
}

string wideStringToUtf8(const wstring& str) {
    return converter.to_bytes(str.c_str());
}

void* vKeyInit() {
    _index = 0;
    _stateIndex = 0;
    _useSpellCheckingBefore = vCheckSpelling;
    _typingStatesData.clear();
    _typingStates.clear();
    _longWordHelper.clear();
    return &HookState;
}

bool isWordBreak(const vKeyEvent& event, const vKeyEventState& state, const Uint16& data) {
    if (event == vKeyEvent::Mouse)
        return true;
    for (i = 0; i < _breakCode.size(); i++) {
        if (_breakCode[i] == data) {
            return true;
        }
    }
    return false;
}

bool isMacroBreakCode(const int& data) {
    for (i = 0; i < _macroBreakCode.size(); i++) {
        if (_macroBreakCode[i] == data) {
            return true;
        }
    }
    return false;
}

void setKeyData(const Byte& index, const Uint16& keyCode, const bool& isCaps) {
    if (index < 0 || index >= MAX_BUFF)
        return;
    TypingWord[index] = keyCode | (isCaps ? CAPS_MASK : 0);
}

bool _spellingOK = false;
bool _spellingFlag = false;
bool _spellingVowelOK = false;
Byte _spellingEndIndex = 0;

void checkSpelling(const bool& forceCheckVowel=false) {
    _spellingOK = false;
    _spellingVowelOK = true;
    _spellingEndIndex = _index;
    
    if (_index > 0 && CHR(_index-1) == KEY_RIGHT_BRACKET) {
        _spellingEndIndex = _index-1;
    }
    
    if (_spellingEndIndex > 0) {
        j = 0;
        //Check first consonant
        if (IS_CONSONANT(CHR(0))) {
            for (i = 0; i < _consonantTable.size(); i++) {
                _spellingFlag = false;
                if (_spellingEndIndex < _consonantTable[i].size())
                    _spellingFlag = true;
                for (j = 0; j < _consonantTable[i].size(); j++) {
                    if (_spellingEndIndex > j &&
                        (_consonantTable[i][j] & ~(vQuickStartConsonant ? END_CONSONANT_MASK : 0)) != CHR(j) &&
                        (_consonantTable[i][j] & ~(vAllowConsonantZFWJ ? CONSONANT_ALLOW_MASK : 0)) != CHR(j)) {
                        _spellingFlag = true;
                        break;
                    }
                }
                if (_spellingFlag)
                    continue;
                
                break;
            }
        }
        
        if (j == _spellingEndIndex){ //for "d" case
            _spellingOK = true;
        }
        
        //check next vowel
        k = j;
        vowelStartIndex = k;
        //August 23rd, 2019: fix case "que't"
        if (CHR(vowelStartIndex) == KEY_U && k > 0 && k < _spellingEndIndex-1 && CHR(vowelStartIndex-1) == KEY_Q) {
            k = k + 1;
            j = k;
            vowelStartIndex = k;
        } else if (_index >= 2 && CHR(0) == KEY_G && CHR(1) == KEY_I && IS_CONSONANT(CHR(2))) {
            vowelStartIndex = k = j = 1; //Sep 28th: fix gìn
        }
        for (l = 0; l < 3; l++) {
            if (k < _spellingEndIndex && !IS_CONSONANT(CHR(k))) {
                k++;
                vowelEndIndex = k;
            }
        }
        if (k > j) { //has vowel,
            _spellingVowelOK = false;
            //check correct combined vowel
            if (k - j > 1 && forceCheckVowel) {
                vector<vector<Uint32>>& vowelSet = _vowelCombine[CHR(j)];
                for (l = 0; l < vowelSet.size(); l++) {
                    _spellingFlag = false;
                    for (ii = 1; ii < vowelSet[l].size(); ii++) {
                        if (j + ii - 1 < _spellingEndIndex && vowelSet[l][ii] != ((CHR(j + ii - 1) | (TypingWord[j + ii - 1] & TONEW_MASK) | (TypingWord[j + ii - 1] & TONE_MASK)))) {
                            _spellingFlag = true;
                            break;
                        }
                    }
                    if (_spellingFlag || (k < _spellingEndIndex && !vowelSet[l][0]) || (j + ii - 1 < _spellingEndIndex && !IS_CONSONANT(CHR(j + ii - 1))))
                        continue;
                    
                    _spellingVowelOK = true;
                    break;
                }
            } else if (!IS_CONSONANT(CHR(j))) {
                _spellingVowelOK = true;
            }
            
            //continue check last consonant
            for (ii = 0; ii < _endConsonantTable.size(); ii++) {
                _spellingFlag = false;
   
                for (j = 0; j < _endConsonantTable[ii].size(); j++) {
                    if (_spellingEndIndex > k+j &&
                        (_endConsonantTable[ii][j] & ~(vQuickEndConsonant ? END_CONSONANT_MASK : 0)) != CHR(k + j)) {
                        _spellingFlag = true;
                        break;
                    }
                }
                if (_spellingFlag)
                    continue;
                
                if (k + j >= _spellingEndIndex) {
                    _spellingOK = true;
                    break;
                }
            }
            
            //limit: end consonant "ch", "t" can not use with "~", "`", "?"
            if (_spellingOK) {
                if (_index >= 3 && CHR(_index-1) == KEY_H && CHR(_index-2) == KEY_C && !((TypingWord[_index-3] & MARK1_MASK) || (TypingWord[_index-3] & MARK5_MASK) || !(TypingWord[_index-3] & MARK_MASK))) {
                    _spellingOK = false;
                } else if (_index >= 2 && CHR(_index-1) == KEY_T && !((TypingWord[_index-2] & MARK1_MASK) || (TypingWord[_index-2] & MARK5_MASK) || !(TypingWord[_index-2] & MARK_MASK))) {
                    _spellingOK = false;
                }
            }
        }
    } else {
        _spellingOK = true;
    }
    tempDisableKey = !(_spellingOK && _spellingVowelOK);
    
    //cout<<"spelling vowel: "<<(_spellingVowelOK ? "OK": "Err")<<endl;
    //cout<<"spelling: "<<(_spellingOK ? "OK": "Err")<<endl<<endl;
}

void checkGrammar(const int& deltaBackSpace) {
    if (_index <= 1 || _index >= MAX_BUFF)
        return;
    
    findAndCalculateVowel(true);
    if (vowelCount == 0)
        return;
    
    isCheckedGrammar = false;
    
    l = vowelStartIndex;
    
    //if N key for case: "thuơn", "ưoi", "ưom", "ưoc"
    if (_index >= 3) {
        for (i = _index-1; i >= 0; i--) {
            if (CHR(i) == KEY_N || CHR(i) == KEY_C || CHR(i) == KEY_I ||
                CHR(i) == KEY_M || CHR(i) == KEY_P || CHR(i) == KEY_T) {
                if (i - 2 >= 0 && CHR(i - 1) == KEY_O && CHR(i - 2) == KEY_U) {
                    if ((TypingWord[i-1] & TONEW_MASK) ^ (TypingWord[i-2] & TONEW_MASK)) {
                        TypingWord[i - 2] |= TONEW_MASK;
                        TypingWord[i - 1] |= TONEW_MASK;
                        isCheckedGrammar = true;
                        break;
                    }
                }
            }
        }
    }
    
    //check mark
    if (_index >= 2) {
        for (i = l; i <= vowelEndIndex; i++) {
            if (TypingWord[i] & MARK_MASK) {
                Uint32 mark = TypingWord[i] & MARK_MASK;
                TypingWord[i] &= ~MARK_MASK;
                insertMark(mark, false);
                if (i != vowelWillSetMark)
                    isCheckedGrammar = true;
                break;
            }
        }
    }
    
    //re-arrange data to sendback
    if (isCheckedGrammar) {
        if (HookState.code ==vDoNothing)
            HookState.code = vWillProcess;
        HookState.backspaceCount = 0;
        
        for (i = _index - 1; i >= l; i--) {
            HookState.backspaceCount++;
            HookState.charData[_index - 1 - i] = getCharacterCode(TypingWord[i]);
        }
        HookState.newCharCount = HookState.backspaceCount;
        HookState.backspaceCount += deltaBackSpace;
        HookState.extCode = 4;
    }
}

void insertKey(const Uint16& keyCode, const bool& isCaps, const bool& isCheckSpelling=true) {
    if (_index >= MAX_BUFF) {
        _longWordHelper.push_back(TypingWord[0]); //save long word
        //left shift
        for (iii = 0; iii < MAX_BUFF - 1; iii++) {
            TypingWord[iii] = TypingWord[iii + 1];
        }
        setKeyData(_index-1, keyCode, isCaps);
    } else {
        setKeyData(_index++, keyCode, isCaps);
    }
    
    if (vCheckSpelling && isCheckSpelling)
        checkSpelling();
    
    //allow d after consonant
    if (keyCode == KEY_D && _index - 2 >= 0 && IS_CONSONANT(CHR(_index - 2)))
        tempDisableKey = false;
}

void insertState(const Uint16& keyCode, const bool& isCaps) {
    if (_stateIndex >= MAX_BUFF) {
        //left shift
        for (iii = 0; iii < MAX_BUFF - 1; iii++) {
            KeyStates[iii] = KeyStates[iii + 1];
        }
        KeyStates[_stateIndex-1] = keyCode | (isCaps ? CAPS_MASK : 0);
    } else {
        KeyStates[_stateIndex++] = keyCode | (isCaps ? CAPS_MASK : 0);
    }
}

void saveWord() {
    //save word history
    if (HookState.code != vReplaceMaro) {
        if (_index > 0) {
            if (_longWordHelper.size() > 0) { //save long word first
                _typingStatesData.clear();
                for (i = 0; i < _longWordHelper.size(); i++) {
                    if (i != 0 && i % MAX_BUFF == 0) { //save if overflow
                        _typingStates.push_back(_typingStatesData);
                        _typingStatesData.clear();
                    }
                    _typingStatesData.push_back(_longWordHelper[i]);
                }
                _typingStates.push_back(_typingStatesData);
                _longWordHelper.clear();
            }
            
            //save current word
            _typingStatesData.clear();
            for (i = 0; i < _index; i++) {
                _typingStatesData.push_back(TypingWord[i]);
            }
            _typingStates.push_back(_typingStatesData);
        }
    } else { //save macro words
        _typingStatesData.clear();
        for (i = 0; i < HookState.macroData.size(); i++) {
            if (i != 0 && i % MAX_BUFF == 0) { //break if overflow
                _typingStates.push_back(_typingStatesData);
                _typingStatesData.clear();
            }
            _typingStatesData.push_back(HookState.macroData[i]);
        }
        _typingStates.push_back(_typingStatesData);
    }
}

void saveWord(const Uint32& keyCode, const int& count) {
    _typingStatesData.clear();
    for (i = 0; i < count; i++) {
        _typingStatesData.push_back(keyCode);
    }
    _typingStates.push_back(_typingStatesData);
}

void saveSpecialChar() {
    _typingStatesData.clear();
    for (i = 0; i < _specialChar.size(); i++) {
        _typingStatesData.push_back(_specialChar[i]);
    }
    _typingStates.push_back(_typingStatesData);
    _specialChar.clear();
}

void restoreLastTypingState() {
    if (_typingStates.size() > 0) {
        _typingStatesData = _typingStates.back();
        _typingStates.pop_back();
        if (_typingStatesData.size() > 0){
            if (_typingStatesData[0] == KEY_SPACE) {
                _spaceCount = (int)_typingStatesData.size();
                _index = 0;
            } else if (std::find(_charKeyCode.begin(), _charKeyCode.end(), (Uint16)_typingStatesData[0]) != _charKeyCode.end()) {
                _index = 0;
                _specialChar = _typingStatesData;
                checkSpelling();
            } else {
                for (i = 0; i < _typingStatesData.size(); i++) {
                    TypingWord[i] = _typingStatesData[i];
                }
                _index = (Byte)_typingStatesData.size();
            }
        }
    }
}

void startNewSession() {
    _index = 0;
    HookState.backspaceCount = 0;
    HookState.newCharCount = 0;
    tempDisableKey = false;
    _stateIndex = 0;
    _hasHandledMacro = false;
    _hasHandleQuickConsonant = false;
    _longWordHelper.clear();
}

void checkCorrectVowel(vector<vector<Uint16>>& charset, int& i, int& k, const Uint16& markKey) {
    //ignore "qu" case
    if (_index >= 2 && CHR(_index-1) == KEY_U && CHR(_index-2) == KEY_Q) {
        isCorect = false;
        return;
    }
    k = _index - 1;
    for (j = (int)charset[i].size() - 1; j >= 0; j--) {
        if ((charset[i][j] & ~(vQuickEndConsonant ? END_CONSONANT_MASK : 0)) != CHR(k)) {
            isCorect = false;
            return;
        }
        k--;
        if (k < 0)
            break;
    }
    
    //limit mark for end consonant: "C", "T"
    if (isCorect && charset[i].size() > 1 && (IS_KEY_F(markKey) || IS_KEY_X(markKey) || IS_KEY_R(markKey))) {
        if (charset[i][1] == KEY_C || charset[i][1] == KEY_T) {
            isCorect = false;
        } else if (charset[i].size() > 2 && (charset[i][2] == KEY_T)) {
            isCorect = false;
        }
    }
    
    if (isCorect && k >= 0) {
        if (CHR(k) == CHR(k+1)) {
            isCorect = false;
        }
    }
}

Uint32 getCharacterCode(const Uint32& data) {
    capsElem = (data & CAPS_MASK) ? 0 : 1;
    key = data & CHAR_MASK;
    if (data & MARK_MASK) { //has mark
        markElem = -2;
        switch (data & MARK_MASK) {
            case MARK1_MASK:
                markElem = 0;
                break;
            case MARK2_MASK:
                markElem = 2;
                break;
            case MARK3_MASK:
                markElem = 4;
                break;
            case MARK4_MASK:
                markElem = 6;
                break;
            case MARK5_MASK:
                markElem = 8;
                break;
        }
        markElem += capsElem;
        
        switch (key) {
            case KEY_A:
            case KEY_O:
            case KEY_U:
            case KEY_E:
                if ((data & TONE_MASK) == 0 && (data & TONEW_MASK) == 0)
                    markElem += 4;
                break;
        }
        
        if (data & TONE_MASK) {
            key |= TONE_MASK;
        } else if (data & TONEW_MASK) {
            key |= TONEW_MASK;
        }
        if (_codeTable[vCodeTable].find(key) == _codeTable[vCodeTable].end())
            return data; //not found
        
        return _codeTable[vCodeTable][key][markElem] | CHAR_CODE_MASK;
    } else { //doesn't has mark
        if (_codeTable[vCodeTable].find(key) == _codeTable[vCodeTable].end())
            return data; //not found
        
        if (data & TONE_MASK) {
            return _codeTable[vCodeTable][key][capsElem] | CHAR_CODE_MASK;
        } else if (data & TONEW_MASK) {
            return _codeTable[vCodeTable][key][capsElem + 2] | CHAR_CODE_MASK;
        } else {
            return data; //not found
        }
    }
    
    return 0;
}

void findAndCalculateVowel(const bool& forGrammar) {
    vowelCount = 0;
    vowelStartIndex = vowelEndIndex = 0;
    for (iii = _index - 1; iii >= 0; iii--) {
        if (IS_CONSONANT(CHR(iii))) {
            if (vowelCount > 0)
                break;
        } else {  //is vowel
            if (vowelCount == 0)
                vowelEndIndex = iii;
            if (!forGrammar) {
                if ((iii-1 >= 0 && (CHR(iii) == KEY_I && CHR(iii-1) == KEY_G)) ||
                    (iii-1 >= 0 && (CHR(iii) == KEY_U && CHR(iii-1) == KEY_Q))) {
                    break;
                }
            }
            vowelStartIndex = iii;
            vowelCount++;
        }
    }
    //August 26th, 2019: don't count "u" at "q u" as a vowel
    if (vowelStartIndex - 1 >= 0 && CHR(vowelStartIndex) == KEY_U && CHR(vowelStartIndex-1) == KEY_Q) {
        vowelStartIndex++;
        vowelCount--;
    }
}

void removeMark() {
    findAndCalculateVowel(true);
    isChanged = false;
    if (_index > 0) {
        for (i = vowelStartIndex; i <= vowelEndIndex; i++) {
            if (TypingWord[i] & MARK_MASK) {
                TypingWord[i] &= ~MARK_MASK;
                isChanged = true;
            }
        }
    }
    if (isChanged) {
        HookState.code = vWillProcess;
        HookState.backspaceCount = 0;
        
        for (i = _index - 1; i >= vowelStartIndex; i--) {
            HookState.backspaceCount++;
            HookState.charData[_index - 1 - i] = getCharacterCode(TypingWord[i]);
        }
        HookState.newCharCount = HookState.backspaceCount;
    } else {
        HookState.code = vDoNothing;
    }
}

bool canHasEndConsonant() {
    vector<vector<Uint32>>& vo = _vowelCombine[CHR(vowelStartIndex)];
    for (ii = 0; ii < vo.size(); ii++) {
        kk = vowelStartIndex;
        for (iii = 1; iii < vo[ii].size(); iii++) {
            if (kk > vowelEndIndex || ((CHR(kk) | (TypingWord[kk] & TONE_MASK) | (TypingWord[kk] & TONEW_MASK)) != vo[ii][iii])) {
                break;
            }
            kk++;
        }
        if (iii >= vo[ii].size()) {
            return vo[ii][0] == 1;
        }
    }
    return false;
}

void handleModernMark() {
    //default
    vowelWillSetMark = vowelEndIndex;
    HookState.backspaceCount = (_index - vowelEndIndex);
    
    //rule 2
    if (vowelCount == 3 && ((CHR(vowelStartIndex) == KEY_O && CHR(vowelStartIndex+1) == KEY_A && CHR(vowelStartIndex+2) == KEY_I) ||
                            (CHR(vowelStartIndex) == KEY_U && CHR(vowelStartIndex+1) == KEY_Y && CHR(vowelStartIndex+2) == KEY_U) ||
                            (CHR(vowelStartIndex) == KEY_O && CHR(vowelStartIndex+1) == KEY_E && CHR(vowelStartIndex+2) == KEY_O) ||
                            (CHR(vowelStartIndex) == KEY_U && CHR(vowelStartIndex+1) == KEY_Y && CHR(vowelStartIndex+2) == KEY_A))) {
        vowelWillSetMark = vowelStartIndex + 1;
        HookState.backspaceCount = _index - vowelWillSetMark;
    } else if ((CHR(vowelStartIndex) == KEY_O && CHR(vowelStartIndex+1) == KEY_I) ||
               (CHR(vowelStartIndex) == KEY_A && CHR(vowelStartIndex+1) == KEY_I) ||
               (CHR(vowelStartIndex)== KEY_U && CHR(vowelStartIndex+1) == KEY_I) ) {
        
        vowelWillSetMark = vowelStartIndex;
        HookState.backspaceCount = _index - vowelWillSetMark;
    } else if (CHR(vowelEndIndex-1) == KEY_A && CHR(vowelEndIndex) == KEY_Y) {
        vowelWillSetMark = vowelEndIndex - 1;
        HookState.backspaceCount = (_index - vowelEndIndex) + 1;
    } else if (CHR(vowelStartIndex) == KEY_U && CHR(vowelStartIndex+1) == KEY_O) {
        vowelWillSetMark = vowelStartIndex + 1;
        HookState.backspaceCount = _index - vowelWillSetMark;
    } else if (CHR(vowelStartIndex+1) == KEY_O || CHR(vowelStartIndex+1) == KEY_U) {
        vowelWillSetMark = vowelEndIndex - 1;
        HookState.backspaceCount = (_index - vowelEndIndex) + 1;
    } else if (CHR(vowelStartIndex) == KEY_O || CHR(vowelStartIndex) == KEY_U) {
        vowelWillSetMark = vowelEndIndex;
        HookState.backspaceCount = (_index - vowelEndIndex);
    }
    
    //rule 3.1
    if ((CHR(vowelStartIndex) == KEY_I && (TypingWord[vowelStartIndex+1] & (KEY_E | TONE_MASK))) ||
        (CHR(vowelStartIndex) == KEY_Y && (TypingWord[vowelStartIndex+1] & (KEY_E | TONE_MASK))) ||
        (CHR(vowelStartIndex) == KEY_U && (TypingWord[vowelStartIndex+1] == (KEY_O | TONE_MASK))) ||
        ((TypingWord[vowelStartIndex] == (KEY_U | TONEW_MASK)) && (TypingWord[vowelStartIndex+1] == (KEY_O | TONEW_MASK)))){
        
        if (vowelStartIndex+2 < _index) {
            if (CHR(vowelStartIndex+2) == KEY_P || CHR(vowelStartIndex+2) == KEY_T ||
                CHR(vowelStartIndex+2) == KEY_M || CHR(vowelStartIndex+2) == KEY_N ||
                CHR(vowelStartIndex+2) == KEY_O || CHR(vowelStartIndex+2) == KEY_U ||
                CHR(vowelStartIndex+2) == KEY_I || CHR(vowelStartIndex+2) == KEY_C ||
                (vowelStartIndex+3 < _index && CHR(vowelStartIndex+2) == KEY_C && CHR(vowelStartIndex+2) == KEY_H) ||
                (vowelStartIndex+3 < _index && CHR(vowelStartIndex+2) == KEY_N && CHR(vowelStartIndex+2) == KEY_H) ||
                (vowelStartIndex+3 < _index && CHR(vowelStartIndex+2) == KEY_N && CHR(vowelStartIndex+2) == KEY_G)) {
                
                vowelWillSetMark = vowelStartIndex + 1;
                HookState.backspaceCount = _index - vowelWillSetMark;
            } else {
                vowelWillSetMark = vowelStartIndex;
                HookState.backspaceCount = _index - vowelWillSetMark;
            }
        } else {
            vowelWillSetMark = vowelStartIndex;
            HookState.backspaceCount = _index - vowelWillSetMark;
        }
    }
    //rule 3.2
    else if ((CHR(vowelStartIndex) == KEY_I && (CHR(vowelStartIndex) == KEY_A)) ||
             (CHR(vowelStartIndex) == KEY_Y && (CHR(vowelStartIndex) == KEY_A)) ||
             (CHR(vowelStartIndex) == KEY_U && (CHR(vowelStartIndex) == KEY_A)) ||
             (CHR(vowelStartIndex) == KEY_U && (TypingWord[vowelStartIndex+1] == (KEY_U | TONEW_MASK)))){
        
        vowelWillSetMark = vowelStartIndex;
        HookState.backspaceCount = _index - vowelWillSetMark;
    }
    
    //rule 4
    if (vowelCount == 2) {
        if (((CHR(vowelStartIndex) == KEY_I) && (CHR(vowelStartIndex+1) == KEY_A)) ||
            ((CHR(vowelStartIndex) == KEY_I) && (CHR(vowelStartIndex+1) == KEY_U)) ||
            ((CHR(vowelStartIndex) == KEY_I) && (CHR(vowelStartIndex+1) == KEY_O))) {
            
            if (vowelStartIndex == 0 || (CHR(vowelStartIndex-1) != KEY_G)) { //dont have G
                vowelWillSetMark = vowelStartIndex;
                HookState.backspaceCount = _index - vowelWillSetMark;
            } else {
                vowelWillSetMark = vowelStartIndex + 1;
                HookState.backspaceCount = _index - vowelWillSetMark;
            }
        } else if ((CHR(vowelStartIndex) == KEY_U) && (CHR(vowelStartIndex+1) == KEY_A)) {
            if (vowelStartIndex == 0 || (CHR(vowelStartIndex-1) != KEY_Q)) { //dont have Q
                if (vowelEndIndex + 1 >= _index || !canHasEndConsonant()) {
                    vowelWillSetMark = vowelStartIndex;
                    HookState.backspaceCount = _index - vowelWillSetMark;
                }
            } else {
                vowelWillSetMark = vowelStartIndex + 1;
                HookState.backspaceCount = _index - vowelWillSetMark;
            }
        } else if ((CHR(vowelStartIndex) == KEY_O) && (CHR(vowelStartIndex+1) == KEY_O)) { //thoong
            vowelWillSetMark = vowelEndIndex;
            HookState.backspaceCount = _index - vowelWillSetMark;
        }
    }
}

void handleOldMark() {
    //default
    if (vowelCount == 0 && CHR(vowelEndIndex) == KEY_I)
        vowelWillSetMark = vowelEndIndex;
    else
        vowelWillSetMark = vowelStartIndex;
    HookState.backspaceCount = (_index - vowelWillSetMark);
    
    //rule 2
    if (vowelCount == 3 || (vowelEndIndex + 1 < _index && IS_CONSONANT(CHR(vowelEndIndex + 1)) && canHasEndConsonant())) {
        vowelWillSetMark = vowelStartIndex + 1;
        HookState.backspaceCount = _index - vowelWillSetMark;
    }
    
    //rule 3
    for (ii = vowelStartIndex; ii <= vowelEndIndex; ii++) {
        if ((CHR(ii) == KEY_E && TypingWord[ii] & TONE_MASK) || (CHR(ii) == KEY_O && TypingWord[ii] & TONEW_MASK)) {
            vowelWillSetMark = ii;
            HookState.backspaceCount = _index - vowelWillSetMark;
            break;
        }
    }
    
    HookState.newCharCount = HookState.backspaceCount;
}

void insertMark(const Uint32& markMask, const bool& canModifyFlag) {
    vowelCount = 0;
    
    if (canModifyFlag)
        HookState.code = vWillProcess;
    HookState.backspaceCount = HookState.newCharCount = 0;
    
    findAndCalculateVowel();
    vowelWillSetMark = 0;
    
    //detect mark position
    if (vowelCount == 1) {
        vowelWillSetMark = vowelEndIndex;
        HookState.backspaceCount = (_index - vowelEndIndex);
    } else { //vowel = 2 or 3
        if (vUseModernOrthography == 0)
            handleOldMark();
        else
            handleModernMark();
        if (TypingWord[vowelEndIndex] & TONE_MASK || TypingWord[vowelEndIndex] & TONEW_MASK)
            vowelWillSetMark = vowelEndIndex;
    }
    
    //send data
    kk = _index - 1 - vowelStartIndex;
    //if duplicate same mark -> restore
    if (TypingWord[vowelWillSetMark] & markMask) {
        
        TypingWord[vowelWillSetMark] &= ~MARK_MASK;
        if (canModifyFlag)
            HookState.code = vRestore;
        for (ii = vowelStartIndex; ii < _index; ii++) {
            TypingWord[ii] &= ~MARK_MASK;
            HookState.charData[kk--] = getCharacterCode(TypingWord[ii]);
        }
        //_index = 0;
        tempDisableKey = true;
    } else {
        //remove other mark
        TypingWord[vowelWillSetMark] &= ~MARK_MASK;
        
        //add mark
        TypingWord[vowelWillSetMark] |= markMask;
        for (ii = vowelStartIndex; ii < _index; ii++) {
            if (ii != vowelWillSetMark) { //remove mark for other vowel
                TypingWord[ii] &= ~MARK_MASK;
            }
            HookState.charData[kk--] = getCharacterCode(TypingWord[ii]);
        }
        
        HookState.backspaceCount = _index - vowelStartIndex;
    }
    HookState.newCharCount = HookState.backspaceCount;
}

void insertD(const Uint16& data, const bool& isCaps) {
    HookState.code = vWillProcess;
    HookState.backspaceCount = 0;
    for (ii = _index - 1; ii >= 0; ii--) {
        HookState.backspaceCount++;
        if (CHR(ii) == KEY_D) { //reverse unicode char
            if (TypingWord[ii] & TONE_MASK) {
                //restore and disable temporary
                HookState.code = vRestore;
                TypingWord[ii] &= ~TONE_MASK;
                HookState.charData[_index - 1 - ii] = TypingWord[ii];
                tempDisableKey = true;
                break;
            } else {
                TypingWord[ii] |= TONE_MASK;
                HookState.charData[_index - 1 - ii] = getCharacterCode(TypingWord[ii]);
            }
            break;
        } else { //preresent old char
            HookState.charData[_index - 1 - ii] = getCharacterCode(TypingWord[ii]);
        }
    }
    HookState.newCharCount = HookState.backspaceCount;
}

void insertAOE(const Uint16& data, const bool& isCaps) {
    findAndCalculateVowel();
    
    //remove W tone
    for (ii = vowelStartIndex; ii <= vowelEndIndex; ii++) {
        TypingWord[ii] &= ~TONEW_MASK;
    }
    
    HookState.code = vWillProcess;
    HookState.backspaceCount = 0;
    
    for (ii = _index - 1; ii >= 0; ii--) {
        HookState.backspaceCount++;
        if (CHR(ii) == data) { //reverse unicode char
            if (TypingWord[ii] & TONE_MASK) {
                //restore and disable temporary
                HookState.code = vRestore;
                TypingWord[ii] &= ~TONE_MASK;
                HookState.charData[_index - 1 - ii] = TypingWord[ii];
                //_index = 0;
                if (data != KEY_O) //case thoòng
                    tempDisableKey = true;
                break;
            } else {
                TypingWord[ii] |= TONE_MASK;
                if (!IS_KEY_D(data))
                    TypingWord[ii] &= ~TONEW_MASK;
                HookState.charData[_index - 1 - ii] = getCharacterCode(TypingWord[ii]);
                
            }
            break;
        } else { //preresent old char
            HookState.charData[_index - 1 - ii] = getCharacterCode(TypingWord[ii]);
        }
    }
    HookState.newCharCount = HookState.backspaceCount;
}

void insertW(const Uint16& data, const bool& isCaps) {
    isRestoredW = false;
    
    findAndCalculateVowel();
    
    //remove ^ tone
    for (ii = vowelStartIndex; ii <= vowelEndIndex; ii++) {
        TypingWord[ii] &= ~TONE_MASK;
    }
    
    if (vowelCount > 1) {
        HookState.backspaceCount = _index - vowelStartIndex;
        HookState.newCharCount = HookState.backspaceCount;
        
        if (((TypingWord[vowelStartIndex] & TONEW_MASK) && (TypingWord[vowelStartIndex+1] & TONEW_MASK)) ||
            ((TypingWord[vowelStartIndex] & TONEW_MASK) && CHR(vowelStartIndex+1) == KEY_I) ||
            ((TypingWord[vowelStartIndex] & TONEW_MASK) && CHR(vowelStartIndex+1) == KEY_A)){
            //restore and disable temporary
            HookState.code = vRestore;
            
            for (ii = vowelStartIndex; ii < _index; ii++) {
                TypingWord[ii] &= ~TONEW_MASK;
                HookState.charData[_index - 1 - ii] = getCharacterCode(TypingWord[ii]) & ~STANDALONE_MASK;
            }
            isRestoredW = true;
            tempDisableKey = true;
        } else {
            HookState.code = vWillProcess;
            
            if ((CHR(vowelStartIndex) == KEY_U && CHR(vowelStartIndex+1) == KEY_O)) {
                if (vowelStartIndex - 2 >= 0 && TypingWord[vowelStartIndex - 2] == KEY_T && TypingWord[vowelStartIndex - 1] == KEY_H) {
                    TypingWord[vowelStartIndex+1] |= TONEW_MASK;
                    if (vowelStartIndex + 2 < _index && CHR(vowelStartIndex+2) == KEY_N) {
                        TypingWord[vowelStartIndex] |= TONEW_MASK;
                    }
                } else if (vowelStartIndex - 1 >= 0 && TypingWord[vowelStartIndex - 1] == KEY_Q) {
                    TypingWord[vowelStartIndex+1] |= TONEW_MASK;
                } else {
                    TypingWord[vowelStartIndex] |= TONEW_MASK;
                    TypingWord[vowelStartIndex+1] |= TONEW_MASK;
                }
            } else if ((CHR(vowelStartIndex) == KEY_U && CHR(vowelStartIndex+1) == KEY_A) ||
                       (CHR(vowelStartIndex) == KEY_U && CHR(vowelStartIndex+1) == KEY_I) ||
                       (CHR(vowelStartIndex) == KEY_U && CHR(vowelStartIndex+1) == KEY_U) ||
                       (CHR(vowelStartIndex) == KEY_O && CHR(vowelStartIndex+1) == KEY_I)) {
                TypingWord[vowelStartIndex] |= TONEW_MASK;
            } else if ((CHR(vowelStartIndex) == KEY_I && CHR(vowelStartIndex+1) == KEY_O) ||
                       (CHR(vowelStartIndex) == KEY_O && CHR(vowelStartIndex+1) == KEY_A)) {
                TypingWord[vowelStartIndex+1] |= TONEW_MASK;
            } else {
                //don't do anything
                tempDisableKey = true;
                isChanged = false;
                HookState.code = vDoNothing;
            }
            
            for (ii = vowelStartIndex; ii < _index; ii++) {
                HookState.charData[_index - 1 - ii] = getCharacterCode(TypingWord[ii]);
            }
        }
        
        return;
    }
    
    HookState.code = vWillProcess;
    HookState.backspaceCount = 0;
    
    for (ii = _index - 1; ii >= 0; ii--) {
        if (ii < vowelStartIndex)
            break;
        HookState.backspaceCount++;
        switch (CHR(ii)) {
            case KEY_A:
            case KEY_U:
            case KEY_O:
                if (TypingWord[ii] & TONEW_MASK) {
                    //restore and disable temporary
                    if (TypingWord[ii] & STANDALONE_MASK) {
                        HookState.code = vWillProcess;
                        if (CHR(ii) == KEY_U){
                            TypingWord[ii] = KEY_W | ((TypingWord[ii] & CAPS_MASK) ? CAPS_MASK : 0);
                        } else if (CHR(ii) == KEY_O) {
                            HookState.code = vRestore;
                            TypingWord[ii] = KEY_O | ((TypingWord[ii] & CAPS_MASK) ? CAPS_MASK : 0);
                            isRestoredW = true;
                        }
                        HookState.charData[_index - 1 - ii] = TypingWord[ii];
                    } else {
                        HookState.code = vRestore;
                        TypingWord[ii] &= ~TONEW_MASK;
                        HookState.charData[_index - 1 - ii] = TypingWord[ii];
                        isRestoredW = true;
                        //_index++;
                    }
                    
                    tempDisableKey = true;
                } else {
                    TypingWord[ii] |= TONEW_MASK;
                    TypingWord[ii] &= ~TONE_MASK;
                    HookState.charData[_index - 1 - ii] = getCharacterCode(TypingWord[ii]);
                }
                break;
                
            default:
                HookState.charData[_index - 1 - ii] = getCharacterCode(TypingWord[ii]);
                break;
        }
    }
    HookState.newCharCount = HookState.backspaceCount;
    
    if (isRestoredW) {
        //_index = 0;
    }
}

void reverseLastStandaloneChar(const Uint32& keyCode, const bool& isCaps) {
    HookState.code = vWillProcess;
    HookState.backspaceCount = 0;
    HookState.newCharCount = 1;
    HookState.extCode = 4;
    TypingWord[_index - 1] = (keyCode | TONEW_MASK | STANDALONE_MASK | (isCaps ? CAPS_MASK : 0));
    HookState.charData[0] = getCharacterCode(TypingWord[_index - 1]);
}

void checkForStandaloneChar(const Uint16& data, const bool& isCaps, const Uint32& keyWillReverse) {
    if (CHR(_index - 1) == keyWillReverse && TypingWord[_index - 1] & TONEW_MASK) {
        HookState.code = vWillProcess;
        HookState.backspaceCount = 1;
        HookState.newCharCount = 1;
        TypingWord[_index - 1] = data | (isCaps ? CAPS_MASK : 0);
        HookState.charData[0] = getCharacterCode(TypingWord[_index - 1]);
        return;
    }
    
    //check standalone w -> ư
    
    if (_index > 0 && CHR(_index-1) == KEY_U && keyWillReverse == KEY_O) {
        insertKey(keyWillReverse, isCaps);
        reverseLastStandaloneChar(keyWillReverse, isCaps);
        return;
    }
    
    if (_index == 0) { //zero char
        insertKey(data, isCaps, false);
        reverseLastStandaloneChar(keyWillReverse, isCaps);
        return;
    } else if (_index == 1) { //1 char
        for (i = 0; i < _standaloneWbad.size(); i++) {
            if (CHR(0) == _standaloneWbad[i]) {
                insertKey(data, isCaps);
                return;
            }
        }
        insertKey(data, isCaps, false);
        reverseLastStandaloneChar(keyWillReverse, isCaps);
        return;
    } else if (_index == 2) {
        for (i = 0; i < _doubleWAllowed.size(); i++) {
            if (CHR(0) == _doubleWAllowed[i][0] && CHR(1) == _doubleWAllowed[i][1]) {
                insertKey(data, isCaps, false);
                reverseLastStandaloneChar(keyWillReverse, isCaps);
                return;
            }
        }
        insertKey(data, isCaps);
        return;
    }
    
    insertKey(data, isCaps);
}

void upperCaseFirstCharacter() {
    if (!(TypingWord[0] & CAPS_MASK)) {
        HookState.code = vWillProcess;
        HookState.backspaceCount = 0;
        HookState.newCharCount = 1;
        TypingWord[0] |= CAPS_MASK;
        HookState.charData[0] = getCharacterCode(TypingWord[0]);
        _upperCaseStatus = 0;
        if (vUseMacro)
            HookState.macroKey[0] |= CAPS_MASK;
    }
}

void handleMainKey(const Uint16& data, const bool& isCaps) {
    //if is Z key, remove mark
    if (IS_KEY_Z(data)) {
        removeMark();
        if (!isChanged) {
            insertKey(data, isCaps);
        }
        return;
    }
    
    if (data == KEY_LEFT_BRACKET) { //standalone key [
        checkForStandaloneChar(data, isCaps, KEY_O);
        return;
    }
    
    if (data == KEY_RIGHT_BRACKET) { //standalone key }
        checkForStandaloneChar(data, isCaps, KEY_U);
        return;
    }
    
    //if is D key
    if (IS_KEY_D(data)) {
        isCorect = false;
        isChanged = false;
        k = _index;
        for (i = 0; i < _consonantD.size(); i++) {
            if (_index < _consonantD[i].size())
                continue;
            isCorect = true;
            checkCorrectVowel(_consonantD, i, k, data);
            
            //allow d after consonant
            if (!isCorect && _index - 2 >= 0 && CHR(_index-1) == KEY_D && IS_CONSONANT(CHR(_index-2))) {
                isCorect = true;
            }
            if (isCorect) {
                isChanged = true;
                insertD(data, isCaps);
                break;
            }
        }
    
        if (!isChanged) {
            insertKey(data, isCaps);
        }
        return;
    }
    
    //if is mark key
    if (IS_MARK_KEY(data)) {
        for (i = 0; i < _vowelForMark.size(); i++) {
            vector<vector<Uint16>>& charset = _vowelForMark[i];
            isCorect = false;
            isChanged = false;
            k = _index;
            for (l = 0; l < charset.size(); l++) {
                if (_index < charset[l].size())
                    continue;
                isCorect = true;
                checkCorrectVowel(charset, l, k, data);
                
                if (isCorect) {
                    isChanged = true;
                    if (IS_KEY_S(data))
                        insertMark(MARK1_MASK);
                    else if (IS_KEY_F(data))
                        insertMark(MARK2_MASK);
                    else if (IS_KEY_R(data))
                        insertMark(MARK3_MASK);
                    else if (IS_KEY_X(data))
                        insertMark(MARK4_MASK);
                    else if (IS_KEY_J(data))
                        insertMark(MARK5_MASK);
                    break;
                }
            }

            if (isCorect) {
                break;
            }
        }
        
        if (!isChanged) {
            insertKey(data, isCaps);
        }
        
        return;
    }
    
    //check Vowel
    if (vInputType == vVNI) {
        for (i = _index-1; i >= 0; i--) {
            if (CHR(i) == KEY_O || CHR(i) == KEY_A || CHR(i) == KEY_E) {
                vowelEndIndex = i;
                break;
            }
        }
    }
    
    keyForAEO = ((vInputType != vVNI) ? data : ((data == KEY_7 || data == KEY_8 ? KEY_W : (data == KEY_6 ? TypingWord[vowelEndIndex] : data))));
    vector<vector<Uint16>>& charset = _vowel[keyForAEO];
    isCorect = false;
    isChanged = false;
    k = _index;
    for (i = 0; i < charset.size(); i++) {
        if (_index < charset[i].size())
            continue;
        isCorect = true;
        checkCorrectVowel(charset, i, k, data);
        
        if (isCorect) {
            isChanged = true;
            if (IS_KEY_DOUBLE(data)) {
                insertAOE(keyForAEO, isCaps);
            } else if (IS_KEY_W(data)) {
                if (vInputType == vVNI) {
                    for (j = _index-1; j >= 0; j--) {
                        if (CHR(j) == KEY_O || CHR(j) == KEY_U ||CHR(j) == KEY_A || CHR(j) == KEY_E) {
                            vowelEndIndex = j;
                            break;
                        }
                    }
                    if ((data == KEY_7 && CHR(vowelEndIndex) == KEY_A && (vowelEndIndex-1>=0 ? CHR(vowelEndIndex-1) != KEY_U : true)) || (data == KEY_8 && (CHR(vowelEndIndex) == KEY_O || CHR(vowelEndIndex) == KEY_U)))
                        break;
                }
                insertW(keyForAEO, isCaps);
            }
            break;
        }
    }
    
    if (!isChanged) {
        if (data == KEY_W && vInputType != vSimpleTelex1) {
            checkForStandaloneChar(data, isCaps, KEY_U);
        } else {
            insertKey(data, isCaps);
        }
    }
}

void handleQuickTelex(const Uint16& data, const bool& isCaps) {
    HookState.code = vWillProcess;
    HookState.backspaceCount = 1;
    HookState.newCharCount = 2;
    HookState.charData[1] = _quickTelex[data][0] | (isCaps ? CAPS_MASK : 0);
    HookState.charData[0] = _quickTelex[data][1] | (isCaps ? CAPS_MASK : 0);
    insertKey(_quickTelex[data][1], isCaps, false);
}

bool checkRestoreIfWrongSpelling(const int& handleCode) {
    for (ii = 0; ii < _index; ii++) {
        if (!IS_CONSONANT(CHR(ii)) &&
            (TypingWord[ii] & MARK_MASK || TypingWord[ii] & TONE_MASK || TypingWord[ii] & TONEW_MASK)) {
            
            HookState.code = handleCode;
            HookState.backspaceCount = _index;
            HookState.newCharCount = _stateIndex;
            for (i = 0; i < _stateIndex; i++) {
                TypingWord[i] = KeyStates[i];
                HookState.charData[_stateIndex - 1 - i] = TypingWord[i];
            }
            _index = _stateIndex;
            return true;
        }
    }
    return false;
}

void vTempOffSpellChecking() {
    if (_useSpellCheckingBefore) {
        vCheckSpelling = vCheckSpelling ? 0 : 1;
    }
}

void vSetCheckSpelling() {
    _useSpellCheckingBefore = vCheckSpelling;
}

void vTempOffEngine(const bool& off) {
    _willTempOffEngine = off;
}

bool checkQuickConsonant() {
    if (_index <= 1) return false;
    l = 0;
    if (_index > 0) {
        if (vQuickStartConsonant && _quickStartConsonant.find(CHR(0)) != _quickStartConsonant.end()) {
            HookState.code = vRestore;
            HookState.backspaceCount = _index;
            HookState.newCharCount = _index + 1;
            if (_index < MAX_BUFF-1)
                _index++;
            //right shift
            for (i = _index-1; i >= 2; i--) {
                TypingWord[i] = TypingWord[i-1];
            }
            TypingWord[1] = _quickStartConsonant[CHR(0)][1] | ((TypingWord[0] & CAPS_MASK) && (TypingWord[2] & CAPS_MASK) ? CAPS_MASK : 0);
            TypingWord[0] = _quickStartConsonant[CHR(0)][0] | (TypingWord[0] & CAPS_MASK ? CAPS_MASK : 0);
            l = 1;;
        }
        if (vQuickEndConsonant &&
            (_index-2 >= 0 && !IS_CONSONANT(CHR(_index-2))) &&
            _quickEndConsonant.find(CHR(_index-1)) != _quickEndConsonant.end()) {
            HookState.code = vRestore;
            if (l == 1) {
                HookState.newCharCount++;
            } else {
                HookState.backspaceCount = 1;
                HookState.newCharCount = 2;
            }
            if (_index < MAX_BUFF-1)
                _index++;
            TypingWord[_index-1] = _quickEndConsonant[CHR(_index-2)][1] | (TypingWord[_index-2] & CAPS_MASK ? CAPS_MASK : 0);
            TypingWord[_index-2] = _quickEndConsonant[CHR(_index-2)][0] | (TypingWord[_index-2] & CAPS_MASK ? CAPS_MASK : 0);
            
            l = 1;
        }
        if (l == 1) {
            _hasHandleQuickConsonant = true;
            for (i = _index - 1; i >= 0; i--) {
                HookState.charData[_index - 1 - i] = getCharacterCode(TypingWord[i]);
            }
            return true;
        }
    }
    return false;
}
/*==========================================================================================================*/

void vEnglishMode(const vKeyEventState& state, const Uint16& data, const bool& isCaps, const bool& otherControlKey) {
    HookState.code = vDoNothing;
    if (state == vKeyEventState::MouseDown || (otherControlKey && !isCaps)) {
        HookState.macroKey.clear();
        _willTempOffEngine = false;
    } else if (data == KEY_SPACE) {
        if (!_hasHandledMacro && findMacro(HookState.macroKey, HookState.macroData)) {
            HookState.code = vReplaceMaro;
            HookState.backspaceCount = (Byte)HookState.macroKey.size();
        }
        HookState.macroKey.clear();
        _willTempOffEngine = false;
    } else if (data == KEY_DELETE) {
        if (HookState.macroKey.size() > 0) {
            HookState.macroKey.pop_back();
        } else {
            _willTempOffEngine = false;
        }
    } else {
        if (isWordBreak(vKeyEvent::Keyboard, state, data) &&
            std::find(_charKeyCode.begin(), _charKeyCode.end(), data) == _charKeyCode.end()) {
            HookState.macroKey.clear();
            _willTempOffEngine = false;
        } else {
            if (!_willTempOffEngine)
                HookState.macroKey.push_back(data | (isCaps ? CAPS_MASK : 0));
        }
    }
}

void vKeyHandleEvent(const vKeyEvent& event,
                     const vKeyEventState& state,
                     const Uint16& data,
                     const Uint8& capsStatus,
                     const bool& otherControlKey) {
    _isCaps = (capsStatus == 1 || //shift
               capsStatus == 2); //caps lock
    if ((IS_NUMBER_KEY(data) && capsStatus == 1)
        || otherControlKey || isWordBreak(event, state, data) || (_index == 0 && IS_NUMBER_KEY(data))) {
        HookState.code = vDoNothing;
        HookState.backspaceCount = 0;
        HookState.newCharCount = 0;
        HookState.extCode = 1; //word break
        
        //check macro feature
        if (vUseMacro && isMacroBreakCode(data) && !_hasHandledMacro && findMacro(HookState.macroKey, HookState.macroData)) {
            HookState.code = vReplaceMaro;
            HookState.backspaceCount = (Byte)HookState.macroKey.size();
            _hasHandledMacro = true;
        } else if ((vQuickStartConsonant || vQuickEndConsonant) && !tempDisableKey && isMacroBreakCode(data)) {
            checkQuickConsonant();
        } else if (vRestoreIfWrongSpelling && isWordBreak(event, state, data)) { //restore key if wrong spelling with break-key
            if (!tempDisableKey && vCheckSpelling) {
                checkSpelling(true); //force check spelling
            }
            if (tempDisableKey && !checkRestoreIfWrongSpelling(vRestoreAndStartNewSession)) {
                HookState.code = vDoNothing;
            }
        }
        
        _isCharKeyCode = state == KeyDown && std::find(_charKeyCode.begin(), _charKeyCode.end(), data) != _charKeyCode.end();
        if (!_isCharKeyCode) { //clear all line cache
            _specialChar.clear();
            _typingStates.clear();
        } else { //check and save current word
            if (_spaceCount > 0) {
                saveWord(KEY_SPACE, _spaceCount);
                _spaceCount = 0;
            } else {
                saveWord();
            }
            _specialChar.push_back(data | (_isCaps ? CAPS_MASK : 0));
            HookState.extCode = 3;//normal word
        }
        
        if (HookState.code == vDoNothing) {
            startNewSession();
            vCheckSpelling = _useSpellCheckingBefore;
            _willTempOffEngine = false;
        } else if (HookState.code == vReplaceMaro || _hasHandleQuickConsonant) {
            _index = 0;
        }
        
        //insert key for macro function
        if (vUseMacro) {
            if (_isCharKeyCode) {
                HookState.macroKey.push_back(data | (_isCaps ? CAPS_MASK : 0));
            } else {
                HookState.macroKey.clear();
            }
        }
        
        if (vUpperCaseFirstChar) {
            if (data == KEY_DOT)
                _upperCaseStatus = 1;
            else if (data == KEY_ENTER || data == KEY_RETURN)
                _upperCaseStatus = 2;
            else
                _upperCaseStatus = 0;
        }
    } else if (data == KEY_SPACE) {
        if (!tempDisableKey && vCheckSpelling) {
            checkSpelling(true); //force check spelling
        }
        if (vUseMacro && !_hasHandledMacro && findMacro(HookState.macroKey, HookState.macroData)) { //macro
            HookState.code = vReplaceMaro;
            HookState.backspaceCount = (Byte)HookState.macroKey.size();
            _spaceCount++;
            _hasHandledMacro = true;
        } else if ((vQuickStartConsonant || vQuickEndConsonant) && !tempDisableKey && checkQuickConsonant()) {
            _spaceCount++;
        } else if (vRestoreIfWrongSpelling && tempDisableKey && !_hasHandledMacro) { //restore key if wrong spelling
            if (!checkRestoreIfWrongSpelling(vRestore)) {
                HookState.code = vDoNothing;
            }
            _spaceCount++;
        } else { //do nothing with SPACE KEY
            HookState.code = vDoNothing;
            _spaceCount++;
        }
        if (vUseMacro) {
            HookState.macroKey.clear();
        }
        if (vUpperCaseFirstChar && _upperCaseStatus == 1) {
            _upperCaseStatus = 2;
        }
        //save word
        if (_spaceCount == 1) {
            if (_specialChar.size() > 0) {
                saveSpecialChar();
            } else {
                saveWord();
            }
        }
        vCheckSpelling = _useSpellCheckingBefore;
        _willTempOffEngine = false;
    } else if (data == KEY_DELETE) {
        HookState.code = vDoNothing;
        HookState.extCode = 2; //delete
        if (_specialChar.size() > 0) {
            _specialChar.pop_back();
            if (_specialChar.size() == 0) {
                restoreLastTypingState();
            }
        } else if (_spaceCount > 0) { //previous char is space
            _spaceCount--;
            if (_spaceCount == 0) { //restore word
                restoreLastTypingState();
            }
        } else {
            if (_stateIndex > 0) {
                _stateIndex--;
            }
            if (_index > 0){
                _index--;
                if (_longWordHelper.size() > 0) {
                    //right shift
                    for (i = MAX_BUFF - 1; i > 0; i--) {
                        TypingWord[i] = TypingWord[i-1];
                    }
                    TypingWord[0] = _longWordHelper.back();
                    _longWordHelper.pop_back();
                    _index++;
                }
                if (vCheckSpelling)
                    checkSpelling();
            }
            if (vUseMacro && HookState.macroKey.size() > 0) {
                HookState.macroKey.pop_back();
            }
            
            HookState.backspaceCount = 0;
            HookState.newCharCount = 0;
            HookState.extCode = 2; //delete key
            if (_index == 0) {
                startNewSession();
                _specialChar.clear();
                restoreLastTypingState();
            } else { //August 23rd continue check grammar
                checkGrammar(1);
            }
        }
    } else { //START AND CHECK KEY
        if (_willTempOffEngine) {
            HookState.code = vDoNothing;
            HookState.extCode = 3;
            return;
        }
        if (_spaceCount > 0) {
            HookState.backspaceCount = 0;
            HookState.newCharCount = 0;
            HookState.extCode = 0;
            startNewSession();
            //continute save space
            saveWord(KEY_SPACE, _spaceCount);
            _spaceCount = 0;
        } else if (_specialChar.size() > 0) {
            saveSpecialChar();
        }

        insertState(data, _isCaps); //save state
        
        if (!IS_SPECIALKEY(data) || tempDisableKey) { //do nothing
            if (vQuickTelex && IS_QUICK_TELEX_KEY(data)) {
                handleQuickTelex(data, _isCaps);
                return;
            } else {
                HookState.code = vDoNothing;
                HookState.backspaceCount = 0;
                HookState.newCharCount = 0;
                HookState.extCode = 3; //normal key
                insertKey(data, _isCaps);
            }
        } else { //check and update key
            //restore state
            HookState.code = vDoNothing;
            HookState.extCode = 3; //normal key
            handleMainKey(data, _isCaps);
        }

        if (!vFreeMark && !IS_KEY_D(data)) {
            if (HookState.code == vDoNothing) {
                checkGrammar(-1);
            } else {
                checkGrammar(0);
            }
        }
        
        if (HookState.code == vRestore) {
            insertKey(data, _isCaps);
            _stateIndex--;
        }
        
        //insert or replace key for macro feature
        if (vUseMacro) {
            if (HookState.code == vDoNothing) {
                HookState.macroKey.push_back(data | (_isCaps ? CAPS_MASK : 0));
            } else if (HookState.code == vWillProcess || HookState.code == vRestore) {
                for (i = 0; i < HookState.backspaceCount; i++) {
                    if (HookState.macroKey.size() > 0) {
                        HookState.macroKey.pop_back();
                    }
                }
                for (i = _index - HookState.backspaceCount; i < HookState.newCharCount + (_index - HookState.backspaceCount); i++) {
                    HookState.macroKey.push_back(TypingWord[i]);
                }
            }
        }
        
        if (vUpperCaseFirstChar) {
            if (_index == 1 && _upperCaseStatus == 2) {
                upperCaseFirstCharacter();
            }
            _upperCaseStatus = 0;
        }
        
        //case [ ]
        if (IS_BRACKET_KEY(data) && (( IS_BRACKET_KEY((Uint16)HookState.charData[0])) || vInputType == vSimpleTelex1 || vInputType == vSimpleTelex2)) {
            if (_index - (HookState.code == vWillProcess ? HookState.backspaceCount : 0) > 0) {
                _index--;
                saveWord();
            }
            _index = 0;
            tempDisableKey = false;
            _stateIndex = 0;
            HookState.extCode = 3;
            _specialChar.push_back(data | (_isCaps ? CAPS_MASK : 0));
        }
    }
    
    //Debug
    //cout<<"index "<<(int)_index<< ", stateIndex "<<(int)_stateIndex<<", word "<<_typingStates.size()<<", long word "<<_longWordHelper.size()<< endl;
    //cout<<"backspace "<<(int)HookState.backspaceCount<<endl;
    //cout<<"new char "<<(int)HookState.newCharCount<<endl<<endl;
}
