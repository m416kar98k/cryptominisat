/*
 * CryptoMiniSat
 *
 * Copyright (c) 2009-2014, Mate Soos. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.0 of the License.
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

#include "stamp.h"
#include "varreplacer.h"
#include "varupdatehelper.h"

using namespace CMSat;

void Stamp::save_on_var_memory(const uint32_t newNumVars)
{
    tstamp.resize(newNumVars*2);
    tstamp.shrink_to_fit();

    for(Timestamp& t: tstamp) {
        Lit lit = t.dominator[STAMP_RED];
        if (lit != lit_Undef
            && lit.var() >= newNumVars
        ) {
            t.dominator[STAMP_RED] = lit_Undef;
        }

        lit = t.dominator[STAMP_IRRED];
        if (lit != lit_Undef
            && lit.var() >= newNumVars
        ) {
            t.dominator[STAMP_IRRED] = lit_Undef;
        }
    }
}

bool Stamp::stampBasedClRem(
    const vector<Lit>& lits
) const {
    StampSorter sortNorm(tstamp, STAMP_IRRED, false);
    StampSorterInv sortInv(tstamp, STAMP_IRRED, false);

    stampNorm = lits;
    stampInv = lits;

    std::stable_sort(stampNorm.begin(), stampNorm.end(), sortNorm);
    std::stable_sort(stampInv.begin(), stampInv.end(), sortInv);

    assert(lits.size() > 0);
    vector<Lit>::const_iterator lpos = stampNorm.begin();
    vector<Lit>::const_iterator lneg = stampInv.begin();

    while(true) {
        if (tstamp[(~*lneg).toInt()].start[STAMP_IRRED]
            >= tstamp[lpos->toInt()].start[STAMP_IRRED]
        ) {
            lpos++;

            if (lpos == stampNorm.end())
                return false;
        } else if (tstamp[(~*lneg).toInt()].end[STAMP_IRRED]
            <= tstamp[lpos->toInt()].end[STAMP_IRRED]
        ) {
            lneg++;

            if (lneg == stampInv.end())
                return false;
        } else {
            return true;
        }
    }

    return false;
}

void Stamp::updateVars(
    const vector<Var>& outerToInter
    , const vector<Var>& interToOuter2
    , vector<uint16_t>& seen
) {
    //Update both dominators
    for(size_t i = 0; i < tstamp.size(); i++) {
        for(size_t i2 = 0; i2 < 2; i2++) {
            if (tstamp[i].dominator[i2] != lit_Undef)
                tstamp[i].dominator[i2]
                    = getUpdatedLit(tstamp[i].dominator[i2], outerToInter);
        }
    }

    //Update the stamp. Stamp can be very large, so update by swapping
    updateBySwap(tstamp, seen, interToOuter2);
}

std::pair<size_t, size_t> Stamp::stampBasedLitRem(
    vector<Lit>& lits
    , StampType stampType
) const {
    size_t remLitTimeStamp = 0;
    StampSorter sorter(tstamp, stampType, true);

    #ifdef DEBUG_STAMPING
    cout << "Ori clause: " << lits << endl;
    #endif

    std::stable_sort(lits.begin(), lits.end(), sorter);

    #ifdef DEBUG_STAMPING
    cout << "sorted clause: " << lits << endl;
    cout << "Timestamps: ";
    for(size_t i = 0; i < lits.size(); i++) {
        cout
        << " " << tstamp[lits[i].toInt()].start[stampType]
        << "," << tstamp[lits[i].toInt()].end[stampType];
    }
    cout << endl;
    #endif

    assert(!lits.empty());
    Lit lastLit = lits[0];
    for(size_t i = 1; i < lits.size(); i++) {
        if (tstamp[lastLit.toInt()].end[stampType]
            < tstamp[lits[i].toInt()].end[stampType]
        ) {
            lits[i] = lit_Undef;
            remLitTimeStamp++;
        } else {
            lastLit = lits[i];
        }
    }

    if (remLitTimeStamp) {
        //First literal cannot be removed
        assert(lits.front() != lit_Undef);

        //At least 1 literal must remain
        assert(remLitTimeStamp < lits.size());

        //Remove lit_Undef-s
        size_t at = 0;
        for(size_t i = 0; i < lits.size(); i++) {
            if (lits[i] != lit_Undef) {
                lits[at++] = lits[i];
            }
        }
        lits.resize(lits.size()-remLitTimeStamp);

        #ifdef DEBUG_STAMPING
        cout << "New clause: " << lits << endl;
        #endif
    }

    size_t remLitTimeStampInv = 0;
    StampSorterInv sorterInv(tstamp, stampType, false);
    std::stable_sort(lits.begin(), lits.end(), sorterInv);
    assert(!lits.empty());
    lastLit = lits[0];

    for(size_t i = 1; i < lits.size(); i++) {
        if (tstamp[(~lastLit).toInt()].end[stampType]
            > tstamp[(~lits[i]).toInt()].end[stampType]
        ) {
            lits[i] = lit_Undef;
            remLitTimeStampInv++;
        } else {
            lastLit = lits[i];
        }
    }

    if (remLitTimeStampInv) {
        //First literal cannot be removed
        assert(lits.front() != lit_Undef);

        //At least 1 literal must remain
        assert(remLitTimeStampInv < lits.size());

        //Remove lit_Undef-s
        size_t at = 0;
        for(size_t i = 0; i < lits.size(); i++) {
            if (lits[i] != lit_Undef) {
                lits[at++] = lits[i];
            }
        }
        lits.resize(lits.size()-remLitTimeStampInv);

        #ifdef DEBUG_STAMPING
        cout << "New clause: " << lits << endl;
        #endif
    }


    return std::make_pair(remLitTimeStamp, remLitTimeStampInv);
}

void Stamp::remove_from_stamps(const Var var)
{
    int types[] = {STAMP_IRRED, STAMP_RED};
    for(int i = 0; i < 2; i++) {
        tstamp[Lit(var, false).toInt()].dominator[types[i]] = lit_Undef;
        tstamp[Lit(var, true).toInt()].dominator[types[i]] = lit_Undef;
    }
    for(size_t i = 0; i < tstamp.size(); i++) {
        for(int i2 = 0; i2 < 2; i2++) {
            if (tstamp[i].dominator[types[i2]].var() == var) {
                tstamp[i].dominator[types[i2]] = lit_Undef;
            }
        }
    }
}

void Stamp::update_dominators(const VarReplacer* replacer)
{
    for(size_t l = 0; l < tstamp.size(); l++) {
        Lit lit = Lit::toLit(l);
        lit = replacer->get_lit_replaced_with(lit);

        //Variable probably eliminated, decomposed, etc. Skip.
        if (lit.toInt() >= tstamp.size())
            continue;

        //Update tstamp to that of the replaced var
        tstamp[l] = tstamp[lit.toInt()];

        for(size_t i2 = 0; i2 < 2; i2++) {
            Lit& dom = tstamp[l].dominator[i2];
            if (dom != lit_Undef) {
                dom = replacer->get_lit_replaced_with(dom);
            }
        }
    }
}

void Stamp::clearStamps()
{
    for(vector<Timestamp>::iterator
        it = tstamp.begin(), end = tstamp.end()
        ; it != end
        ; it++
    ) {
        *it = Timestamp();
    }
}
