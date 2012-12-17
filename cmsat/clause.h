/*
 * CryptoMiniSat
 *
 * Copyright (c) 2009-2011, Mate Soos and collaborators. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3.0 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
*/

#ifndef CLAUSE_H
#define CLAUSE_H

#include <cstdio>
#include <vector>
#include <sys/types.h>
#include <string.h>
#include <limits>

#include "solvertypes.h"
#include "constants.h"
#include "watched.h"
#include "alg.h"
#include "clabstraction.h"
#include "constants.h"

class ClauseAllocator;

struct ResolutionTypes
{
    ResolutionTypes() :
        binCl(0)
        , triCl(0)
        , longCl(0)
    {}

    uint32_t sum() const
    {
        return binCl + triCl + longCl;
    }

    ResolutionTypes& operator+=(const ResolutionTypes& other)
    {
        binCl += other.binCl;
        triCl += other.triCl;
        longCl += other.longCl;

        return *this;
    }

    uint16_t binCl;
    uint16_t triCl;
    uint16_t longCl;
};

struct ClauseStats
{
    ClauseStats() :
        glue(std::numeric_limits<uint16_t>::max())
        , activity(0)
        , conflictNumIntroduced(std::numeric_limits<uint32_t>::max())
        , numProp(0)
        , numConfl(0)
        , numLitVisited(0)
        , numLookedAt(0)
        , numUsedUIP(0)
    {}

    uint32_t numPropAndConfl() const
    {
        return numProp + numConfl;
    }

    //Stored data
    uint16_t glue;    ///<Clause glue
    double   activity;
    uint32_t conflictNumIntroduced; ///<At what conflict number the clause  was introduced
    uint32_t numProp; ///<Number of times caused propagation
    uint32_t numConfl; ///<Number of times caused conflict
    uint32_t numLitVisited; ///<Number of literals visited
    uint32_t numLookedAt; ///<Number of times the clause has been deferenced during propagation
    uint32_t numUsedUIP; ///Number of times the claue was using during 1st UIP conflict generation

    ///Number of resolutions it took to make the clause when it was
    ///originally learnt. Only makes sense for learnt clauses
    ResolutionTypes resolutions;

    void clearAfterReduceDB()
    {
        activity = 0;
        numProp = 0;
        numConfl = 0;
        numLitVisited = 0;
        numLookedAt = 0;
        numUsedUIP = 0;
    }

    static ClauseStats combineStats(const ClauseStats& first, const ClauseStats& second)
    {
        //Create to-be-returned data
        ClauseStats ret;

        //Combine stats
        ret.glue = std::min(first.glue, second.glue);
        ret.conflictNumIntroduced = std::min(first.conflictNumIntroduced, second.conflictNumIntroduced);
        ret.numProp = first.numProp + second.numProp;
        ret.numConfl = first.numConfl + second.numConfl;
        ret.numLitVisited = first.numLitVisited + second.numLitVisited;
        ret.numLookedAt = first.numLookedAt + second.numLookedAt;
        ret.numUsedUIP = first.numUsedUIP + second.numUsedUIP;

        return ret;
    };
};

inline std::ostream& operator<<(std::ostream& os, const ClauseStats& stats)
{

    os << "glue " << stats.glue << " ";
    os << "conflIntro " << stats.conflictNumIntroduced<< " ";
    os << "numProp " << stats.numProp<< " ";
    os << "numConfl " << stats.numConfl<< " ";
    os << "numLitVisit " << stats.numLitVisited<< " ";
    os << "numLook " << stats.numLookedAt<< " ";
    os << "numUsedUIP" << stats.numUsedUIP << " ";

    return os;
}

/**
@brief Holds a clause. Does not allocate space for literals

Literals are allocated by an external allocator that allocates enough space
for the class that it can hold the literals as well. I.e. it malloc()-s
    sizeof(Clause)+LENGHT*sizeof(Lit)
to hold the clause.
*/
struct Clause
{
protected:

    uint32_t isLearnt:1; ///<Is the clause a learnt clause?
    uint32_t strenghtened:1; ///<Has the clause been strenghtened since last simplification?
    uint32_t changed:1; ///<Var inside clause has been changed

    uint32_t isRemoved:1; ///<Is this clause queued for removal because of usless binary removal?
    uint32_t isFreed:1; ///<Has this clause been marked as freed by the ClauseAllocator ?
    uint16_t mySize; ///<The current size of the clause


    Lit* getData()
    {
        return (Lit*)((char*)this + sizeof(Clause));
    }

    const Lit* getData() const
    {
        return (Lit*)((char*)this + sizeof(Clause));
    }

public:
    char defOfOrGate; //TODO make it into a bitfield above
    CL_ABST_TYPE abst;
    ClauseStats stats;

    template<class V>
    Clause(const V& ps, const uint32_t _conflictNumIntroduced)
    {
        assert(ps.size() > 2);

        stats.conflictNumIntroduced = _conflictNumIntroduced;
        stats.glue = std::min<uint16_t>(stats.glue, ps.size());
        defOfOrGate = false;
        isFreed = false;
        mySize = ps.size();
        isLearnt = false;
        isRemoved = false;

        for (uint32_t i = 0; i < ps.size(); i++)
            getData()[i] = ps[i];

        setChanged();
    }

    typedef Lit* iterator;
    typedef const Lit* const_iterator;

    friend class ClauseAllocator;

    uint16_t size() const
    {
        return mySize;
    }

    bool getChanged() const
    {
        return changed;
    }

    void setChanged()
    {
        setStrenghtened();
        changed = 1;
    }

    void unsetChanged()
    {
        changed = 0;
    }

    void shrink (const uint32_t i)
    {
        assert(i <= size());
        mySize -= i;
        if (i > 0)
            setStrenghtened();
    }

    void resize (const uint32_t i)
    {
        assert(i <= size());
        if (i == size()) return;
        mySize = i;
        setStrenghtened();
    }

    bool learnt() const
    {
        return isLearnt;
    }

    bool freed() const
    {
        return isFreed;
    }

    bool getStrenghtened() const
    {
        return strenghtened;
    }

    void reCalcAbstraction()
    {
        abst = calcAbstraction(*this);
    }

    void setStrenghtened()
    {
        abst = calcAbstraction(*this);
        strenghtened = true;
    }

    void unsetStrenghtened()
    {
        strenghtened = false;
    }

    Lit& operator [] (const uint32_t i)
    {
        return *(getData() + i);
    }

    const Lit& operator [] (const uint32_t i) const
    {
        return *(getData() + i);
    }

    void makeNonLearnt()
    {
        assert(isLearnt);
        isLearnt = false;
    }

    void makeLearnt(const uint32_t newGlue)
    {
        stats.glue = newGlue;
        isLearnt = true;
    }

    void strengthen(const Lit p)
    {
        remove(*this, p);
        setStrenghtened();
    }

    void add(const Lit p)
    {
        mySize++;
        getData()[mySize-1] = p;
        setChanged();
    }

    const Lit* begin() const
    {
        return getData();
    }

    Lit* begin()
    {
        return getData();
    }

    const Lit* end() const
    {
        return getData()+size();
    }

    Lit* end()
    {
        return getData()+size();
    }

    void setRemoved()
    {
        isRemoved = true;
    }

    bool getRemoved() const
    {
        return isRemoved;
    }

    void setFreed()
    {
        isFreed = true;
    }

    bool getFreed() const
    {
        return isFreed;
    }

    void combineStats(const ClauseStats& other)
    {
        stats = ClauseStats::combineStats(stats, other);
    }
};

inline std::ostream& operator<<(std::ostream& os, const Clause& cl)
{
    for (uint32_t i = 0; i < cl.size(); i++) {
        os << cl[i];

        if (i+1 == cl.size())
            os << " ";
    }

    return os;
}

struct ClauseUsageStats
{
    ClauseUsageStats() :
        num(0)
        , sumProp(0)
        , sumConfl(0)
        , sumLitVisited(0)
        , sumLookedAt(0)
        , sumUsedUIP(0)
    {}

    uint64_t sumPropAndConfl() const
    {
        return sumProp + sumConfl;
    }

    uint64_t num;
    uint64_t sumProp;
    uint64_t sumConfl;
    uint64_t sumLitVisited;
    uint64_t sumLookedAt;
    uint64_t sumUsedUIP;

    ClauseUsageStats& operator+=(const ClauseUsageStats& other)
    {
        num += other.num;
        sumProp += other.sumProp;
        sumConfl += other.sumConfl;
        sumLitVisited += other.sumLitVisited;
        sumLookedAt += other.sumLookedAt;
        sumUsedUIP += other.sumUsedUIP;

        return *this;
    }

    void addStat(const Clause& cl)
    {
        num++;
        sumProp += cl.stats.numProp;
        sumConfl += cl.stats.numConfl;
        sumLitVisited += cl.stats.numLitVisited;
        sumLookedAt += cl.stats.numLookedAt;
        sumUsedUIP += cl.stats.numUsedUIP;
    }
};

enum clauseCleaningTypes {
    CLEAN_CLAUSES_GLUE_BASED
    , CLEAN_CLAUSES_SIZE_BASED
    , CLEAN_CLAUSES_PROPCONFL_BASED
    ,  CLEAN_CLAUSES_ACTIVITY_BASED
};

inline std::string getNameOfCleanType(clauseCleaningTypes clauseCleaningType)
{
    switch(clauseCleaningType) {
        case CLEAN_CLAUSES_GLUE_BASED :
            return "glue";

        case CLEAN_CLAUSES_SIZE_BASED:
            return "size";

        case CLEAN_CLAUSES_PROPCONFL_BASED:
            return "propconfl";

        case CLEAN_CLAUSES_ACTIVITY_BASED:
            return "activity";

        default:
            assert(false && "Unknown clause cleaning type?");
    };

    return "";
}

struct CleaningStats
{
    struct Data
    {
        Data() :
            num(0)
            , lits(0)
            , age(0)

            , glue(0)
            , numProp(0)
            , numConfl(0)
            , numLitVisited(0)
            , numLookedAt(0)
            , numUsedUIP(0)
            , resolutionsBin(0)
            , resolutionsTri(0)
            , resolutionsLong(0)

            , act(0)
        {}

        uint64_t sumResolutions() const
        {
            return resolutionsBin + resolutionsTri + resolutionsLong;
        }

        Data& operator+=(const Data& other)
        {
            num += other.num;
            lits += other.lits;
            age += other.age;

            glue += other.glue;
            numProp += other.numProp;
            numConfl += other.numConfl;
            numLitVisited += other.numLitVisited;
            numLookedAt += other.numLookedAt;
            numUsedUIP += other.numUsedUIP;
            resolutionsBin += other.resolutionsBin;
            resolutionsTri += other.resolutionsTri;
            resolutionsLong += other.resolutionsLong;

            act += other.act;

            return *this;
        }

        uint64_t num;
        uint64_t lits;
        uint64_t age;

        uint64_t glue;
        uint64_t numProp;
        uint64_t numConfl;
        uint64_t numLitVisited;
        uint64_t numLookedAt;
        uint64_t numUsedUIP;
        uint64_t resolutionsBin;
        uint64_t resolutionsTri;
        uint64_t resolutionsLong;
        double   act;

    };
    CleaningStats() :
        cpu_time(0)
        //Before remove
        , origNumClauses(0)
        , origNumLits(0)

        //Type of clean
        , glueBasedClean(0)
        , sizeBasedClean(0)
        , propConflBasedClean(0)
        , actBasedClean(0)
    {}

    CleaningStats& operator+=(const CleaningStats& other)
    {
        //Time
        cpu_time += other.cpu_time;

        //Before remove
        origNumClauses += other.origNumClauses;
        origNumLits += other.origNumLits;

        //Type of clean
        glueBasedClean += other.glueBasedClean;
        sizeBasedClean += other.sizeBasedClean;
        propConflBasedClean += other.propConflBasedClean;
        actBasedClean += other.actBasedClean;

        //Clause Cleaning data
        preRemove += other.preRemove;
        removed += other.removed;
        remain += other.remain;

        return *this;
    }

    void print(const size_t nbReduceDB) const
    {
        cout << "c ------ CLEANING STATS ---------" << endl;
        //Pre-clean
        printStatsLine("c pre-removed"
            , preRemove.num
            , (double)preRemove.num/(double)origNumClauses*100.0
            , "% long learnt clauses"
        );

        printStatsLine("c pre-removed lits"
            , preRemove.lits
            , (double)preRemove.lits/(double)origNumLits*100.0
            , "% long learnt lits"
        );
        printStatsLine("c pre-removed cl avg size"
            , (double)preRemove.lits/(double)preRemove.num
        );
        printStatsLine("c pre-removed cl avg glue"
            , (double)preRemove.glue/(double)preRemove.num
        );
        printStatsLine("c pre-removed cl avg num resolutions"
            , (double)preRemove.sumResolutions()/(double)preRemove.num
        );

        //Types of clean
        printStatsLine("c clean by glue"
            , glueBasedClean
            , (double)glueBasedClean/(double)nbReduceDB*100.0
            , "% cleans"
        );
        printStatsLine("c clean by size"
            , sizeBasedClean
            , (double)sizeBasedClean/(double)nbReduceDB*100.0
            , "% cleans"
        );
        printStatsLine("c clean by prop&confl"
            , propConflBasedClean
            , (double)propConflBasedClean/(double)nbReduceDB*100.0
            , "% cleans"
        );

        //--- Actual clean --

        //-->CLEAN
        printStatsLine("c cleaned cls"
            , removed.num
            , (double)removed.num/(double)origNumClauses*100.0
            , "% long learnt clauses"
        );
        printStatsLine("c cleaned lits"
            , removed.lits
            , (double)removed.lits/(double)origNumLits*100.0
            , "% long learnt lits"
        );
        printStatsLine("c cleaned cl avg size"
            , (double)removed.lits/(double)removed.num
        );
        printStatsLine("c cleaned avg glue"
            , (double)removed.glue/(double)removed.num
        );

        //--> REMAIN
        printStatsLine("c remain cls"
            , remain.num
            , (double)remain.num/(double)origNumClauses*100.0
            , "% long learnt clauses"
        );
        printStatsLine("c remain lits"
            , remain.lits
            , (double)remain.lits/(double)origNumLits*100.0
            , "% long learnt lits"
        );
        printStatsLine("c remain cl avg size"
            , (double)remain.lits/(double)remain.num
        );
        printStatsLine("c remain avg glue"
            , (double)remain.glue/(double)remain.num
        );

        cout << "c ------ CLEANING STATS END ---------" << endl;
    }

    void printShort() const
    {
        //Pre-clean
        cout
        << "c [DBclean]"
        << " Pre-removed: "
        << preRemove.num
        << " next by " << getNameOfCleanType(clauseCleaningType)
        << endl;

        cout
        << "c [DBclean]"
        << " rem " << removed.num

        << " avgGlue " << std::fixed << std::setprecision(2)
        << ((double)removed.glue/(double)removed.num)

        << " avgSize "
        << std::fixed << std::setprecision(2)
        << ((double)removed.lits/(double)removed.num)
        << endl;

        cout
        << "c [DBclean]"
        << " remain " << remain.num

        << " avgGlue " << std::fixed << std::setprecision(2)
        << ((double)remain.glue/(double)remain.num)

        << " avgSize " << std::fixed << std::setprecision(2)
        << ((double)remain.lits/(double)remain.num)
        << endl;
    }

    //Time
    double cpu_time;

    //Before remove
    uint64_t origNumClauses;
    uint64_t origNumLits;

    //Clause Cleaning --pre-remove
    Data preRemove;

    //Clean type
    clauseCleaningTypes clauseCleaningType;
    size_t glueBasedClean;
    size_t sizeBasedClean;
    size_t propConflBasedClean;
    size_t actBasedClean;

    //Clause Cleaning
    Data removed;
    Data remain;
};

#endif //CLAUSE_H
