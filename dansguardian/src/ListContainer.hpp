//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.
//Written by Daniel Barron (daniel@/ jadeb.com).
//For support go to http://groups.yahoo.com/group/dansguardian

//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef __HPP_LISTCONTAINER
#define __HPP_LISTCONTAINER
#include <deque>
#include <string>
#include "String.hpp"

class ListContainer {

public:
    ListContainer();
    ~ListContainer();

    void reset();

    bool readPhraseList(const char* filename, bool isexception);
    bool readItemList(const char* filename, bool startswith, int filters);

    bool inList(char* string);
    bool inListEndsWith(char* string);
    bool inListStartsWith(char* string);

    char* findInList(char* string);

    char* findEndsWith(char* string);
    char* findStartsWith(char* string);
    char* findStartsWithPartial(char* string);


    int getListLength() {return items;}
    std::string getItemAt(char* index);
    std::string getItemAtInt(int index);

    int getWeightAt(unsigned int index);
    int getTypeAt(unsigned int index);


    void endsWithSort();
    void startsWithSort();

    bool createCacheFile();
    void makeGraph(int fqs);

    bool previousUseItem(const char* filename, bool startswith, int filters);
    bool upToDate();

    std::deque<unsigned int> graphSearch(char* doc, int len);
    std::deque<int> combilist;
    int refcount;
    bool parent;
    int filedate;
    bool used;
    String bannedpfile;
    String exceptionpfile;
    String weightedpfile;
    int bannedpfiledate;
    int exceptionpfiledate;
    int weightedpfiledate;
    String sourcefile;  // used for non-phrase lists only
    std::deque<unsigned int> morelists;  // has to be non private as reg
    // exp compiler needs to access these

private:

    bool sourceisexception;
    bool sourcestartswith;
    int sourcefilters;
    char* data;
    int* graphdata;
    int graphitems;
    std::deque<unsigned int> slowgraph;
    unsigned int data_length;
    unsigned int data_memory;
    int items;
    bool isSW;
    bool issorted;
    bool graphused;
    std::deque<unsigned int> list;
    std::deque<int> weight;
    std::deque<int> itemtype;  // 0=banned, 1=weighted, -1=exception
    int force_quick_search;


    bool readAnotherItemList(const char* filename, bool startswith, int filters);

    void readPhraseListHelper(String line, bool isexception);
    void readPhraseListHelper2(String phrase, int type, int weighting);
    bool addToItemListPhrase(char* s, int len, int type, int weighting, bool combi);
    void graphSizeSort(int l, int r, std::deque<unsigned int>* sizelist);
    void graphAdd(String s, int inx, int item);
    int graphFindBranches(unsigned int pos);
    void graphCopyNodePhrases(unsigned int pos);
    int bmsearch(char* file, int fl, std::string s);
    void quicksortSW(int l, int r);
    void quicksortEW(int l, int r);
    bool readProcessedItemList(const char* filename, bool startswith, int filters);
    void addToItemList(char* s, int len);
    int greaterThanEWF(const char* a, const char* b);  // full match
    int greaterThanEW(const char* a, const char* b);  // partial ends with
    int greaterThanSWF(const char* a, const char* b);  // full match
    int greaterThanSW(const char* a, const char* b);  // partial starts with
    int searchREWF(int a, int s, const char* p);
    int searchREW(int a, int s, const char* p);
    int searchRSW(int a, int s, const char* p);
    int searchRSWF(int a, int s, const char* p);
    bool isCacheFileNewer(const char* string);
    int getFileLength(const char* filename);
    int getFileDate(const char* filename);
    void increaseMemoryBy(int bytes);
    std::string toLower(std::string s);
};

#endif
