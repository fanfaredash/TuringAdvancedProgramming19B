#include "rule.h"
#include "apriori.h"
#include "calcu.h"
#include <iostream>
#include <iomanip>

using namespace llvm;
using namespace std;
using namespace miner;

static Info DebugInfoTuple;

GenRule::GenRule(vector<ItemSet> FIs, vector<ItemSet> IIs, double min_conf, SPT_calc::SupportInfo *_supportInfoAddr) {
    supportInfoAddr = _supportInfoAddr;
    for(auto I: FIs) {
        if(I.size() < 2) continue;
        for(unsigned int i = 0; i < I.size(); i++) {
            if(I.size() == 2 && I[0] == I[1]) continue; //check_same
            auto A = I;
            swap(A[i], A.back());
            A.pop_back();
            int v1 = supportInfoAddr -> calcu_support(&I), v2 = supportInfoAddr -> calcu_support(&A);
            double conf = 1.0 * v1 / v2;
            if(conf >= min_conf)
                PARs.push_back(make_tuple(conf, I, A, I[i]));
        }
    }
    std::sort(PARs.begin(), PARs.end(), cmp_Rule);

    //IIs
    for(auto I: IIs) {
        if(I.size() < 2) continue;
        //sort(I);
        Rule temp = make_tuple(1.14514, ItemSet(), ItemSet(), string());
        for(unsigned int i = 0; i < I.size(); i++) {
            auto A = I;
            swap(A[i], A.back());
            A.pop_back();
            int v1 = supportInfoAddr -> calcu_support(&I), v2 = supportInfoAddr -> calcu_support(&A);
            if(v2 == 0) continue;
            double conf = 1.0 - 1.0 * v1 / v2;
            if(conf <= get<0>(temp)) temp = make_tuple(conf, I, A, I[i]);
        }
        if(1.0 >= get<0>(temp) && get<0>(temp) >= min_conf) NARs.push_back(temp);
    }
    std::sort(NARs.begin(), NARs.end(), cmp_Rule);
    cout << "$PAR\n";
    cout.flush();
    for(auto R: PARs) {
        //if(get<0>(R) == 1.0) continue;
        double conf = get<0>(R);
        auto A = get<2>(R);
        auto b = get<3>(R);
        for(int aa = 0; aa < A.size(); ++aa) {
            auto a = A[aa];
            if(aa < A.size() - 1) cout << a << "#", cout.flush();
            else cout << a, cout.flush();
        }
        cout << "^" << b << "|" << fixed << setprecision(1) << conf * 100 << "%\n";
        cout.flush();
    }
    cout << "$$PAR\n";
    cout.flush();
    cout << "$NAR\n";
    cout.flush();
    for(auto R: NARs) {
        //if(get<0>(R) == 1.0) continue;
        double conf = get<0>(R);
        auto A = get<2>(R);
        auto b = get<3>(R);
        for(int aa = 0; aa < A.size(); ++aa) {
            auto a = A[aa];
            if(aa < A.size() - 1) cout << a << "#", cout.flush();
            else cout << a, cout.flush();
        }
        cout << "^" << b << "|" << fixed << setprecision(1) << conf * 100 << "%\n";
        cout.flush();
    }
    cout << "$$NAR\n";
    cout.flush();
}

vector<Rule>& GenRule::getPARs() { return PARs; }

vector<Rule>& GenRule::getNARs() { return NARs; }

Info &retrieveDebugInfo(Instruction* inst) {
    DebugInfoTuple = {0, "[Unknown]", "[Unknown]", 0}; //Failure display shows linker error
    const DebugLoc &debugLoc = inst -> getDebugLoc();
    if(debugLoc) {
        unsigned int instLine = debugLoc.getLine();
        get<0>(DebugInfoTuple) = instLine;
        MDNode *scope = debugLoc.getScope();
        if(isa<DISubprogram>(scope)) {
            DISubprogram *disp = cast<DISubprogram>(scope);
            get<1>(DebugInfoTuple) = disp -> getDirectory().str();
            get<2>(DebugInfoTuple) = disp -> getFilename().str();
            unsigned int funcLine = disp -> getLine();
            get<3>(DebugInfoTuple) = funcLine;
        }
    }
    return DebugInfoTuple;
}

void DetectRule(GenRule &generator, SPT_calc::SupportInfo *spt) {
    static unsigned int errorcount = 0;
    vector<Rule>& NARs = generator.getNARs();
    vector<Rule>& PARs = generator.getPARs();
    cout << "$RES" << endl;
    for(auto R : PARs) {
        if(get<0>(R) >= 1 - 1e-6) continue;
        vector<string> A = get<2>(R);
        vector<string> B = get<1>(R);
        vector<vector<vector<Instruction*>>> vios = spt->get_PARs_violations(&A , &B);
        for(auto funcvec : vios) {
            for(auto res : funcvec) {
                errorcount++;
                // cout << "Error #" << errorcount << ": \n";
                // cout.flush();
                double conf = get<0>(R);
                cout << "$X" << fixed << setprecision(1) << conf * 100 << "|", cout.flush();
                for(auto inst : res) {
                    DisplayInst(inst);
                }
                cout << get<3>(R) << "\n";
                cout.flush();
            }
            //if(!funcvec.empty()) cout << "\n";
        } 
    }
    for(auto R : NARs) {
        if(get<0>(R) >= 1 - 1e-6) continue;
        vector<string> B = get<1>(R);

        vector<vector<vector<Instruction*>>> vios = spt->get_NARs_violations(&B);
        for(auto funcvec : vios) {
            for(auto res : funcvec) {
                
                if(get<2>(R).size() == 1 && get<2>(R)[0] == get<3>(R)) {
                    errorcount++;
                    // cout << "Error #" << errorcount << ": \n";
                    double conf = get<0>(R);
                    cout << "$Y" << fixed << setprecision(1) << conf * 100 << "|", cout.flush();
                    char sep = '#';
                    for(auto inst : res) {
                        DisplayInst(inst, sep);
                        if(inst == res[0]) sep = '$';
                    }
                    cout << "\n";
                    cout.flush();
                }
                else {
                    errorcount++;
                    // cout << "Error #" << errorcount << ": \n";
                    double conf = get<0>(R);
                    cout << "$Y" << fixed << setprecision(1) << conf * 100 << "|", cout.flush();
                    for(auto inst : res) {
                        if(normalizeInstruction(inst) == get<3>(R)) continue;
                        DisplayInst(inst);
                    }
                    for(auto inst : res) {
                        if(normalizeInstruction(inst) != get<3>(R)) continue;
                        DisplayInst(inst, '$');
                    }
                    cout << "\n";
                    cout.flush();
                }
            }
            //if(!funcvec.empty()) cout << "\n";
        } 
    }
    cout << "$$RES" << endl;
}

void DisplayInst(Instruction* inst, char sep) {
    Info i = retrieveDebugInfo(inst);
    outs() << *inst << "&" << get<0>(i) << sep;
    outs().flush();
}