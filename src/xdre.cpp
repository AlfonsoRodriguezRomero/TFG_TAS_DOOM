#include <vector>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <sstream>
#include "xdre.hpp"
#include <list>

//incluye el fichero con el algoritmo genetico
#include "genetico/Simple-Genetic-Algorithm-master/genetic-algorithm.h"
//#include "genetico/Simple-Genetic-Algorithm-master/genetico.cpp"
//#include "/home/alf/Documents/TAS/xdre-master/src/genetico/Simple-Genetic-Algorithm-master/genetico.cpp"


//from main.h, because I'm lazy
extern int mainArgc;
extern char** mainArgv;

//doom is booby trapped for c++ compilers so I don't really want to include any headers.
//Instead, copypaste and extern what we need and move rest (anything with dbooleans) to cstuff
namespace {
enum skill_t {
    sk_none = -1,
    sk_baby = 0,
    sk_easy,
    sk_medium,
    sk_hard,
    sk_nightmare
};

enum gamestate_t {
    GS_LEVEL,
    GS_INTERMISSION,
    GS_FINALE,
    GS_DEMOSCREEN
};

enum complevel_t_e {
    doom_12_compatibility,
    doom_1666_compatibility,
    doom2_19_compatibility,
    ultdoom_compatibility,
    finaldoom_compatibility,
    dosdoom_compatibility,
    tasdoom_compatibility,
    boom_compatibility_compatibility,
    boom_201_compatibility,
    boom_202_compatibility,
    lxdoom_1_compatibility,
    mbf_compatibility,
    prboom_1_compatibility,
    prboom_2_compatibility,
    prboom_3_compatibility,
    prboom_4_compatibility,
    prboom_5_compatibility,
    prboom_6_compatibility,
    MAX_COMPATIBILITY_LEVEL,
    boom_compatibility = boom_201_compatibility,
    best_compatibility = prboom_6_compatibility,
};

enum buttoncode_t {
    BT_ATTACK       = 1,
    BT_USE          = 2,
    BT_SPECIAL      = 128,
    BT_SPECIALMASK  = 3,
    BT_CHANGE       = 4,
    BT_WEAPONMASK_OLD   = (8+16+32),
    BT_WEAPONMASK   = (8+16+32+64),
    BT_WEAPONSHIFT  = 3,
    BTS_LOADGAME    = 0,
    BTS_PAUSE       = 1,
    BTS_SAVEGAME    = 2,
    BTS_RESTARTLEVEL= 3,
    BTS_SAVEMASK    = (4+8+16),
    BTS_SAVESHIFT   = 2,
};

struct ticcmd_t {
    signed char forwardmove;
    signed char sidemove;
    signed short angleturn;
    short consistancy;
    unsigned char chatchar;
    unsigned char buttons;
};

extern "C" {
#include "cstuff.h"
void initDoom(int argc, char** argv);
void D_Display(void);
void G_InitNew(skill_t skill, int episode, int map);
unsigned char const* G_ReadDemoHeader(unsigned char const* demo_p, std::size_t size);
void G_WriteDemoTiccmd(ticcmd_t const* cmd, FILE* file);
void G_Ticker(void);
void I_StartTic(void);
void M_Ticker(void);
void ST_Start(void);
void HU_Start(void);

//
ticcmd_t netcmds[4];
gamestate_t gamestate;
int leveltime;
int consoleplayer;
int compatibility_level; //complevel_t = int
int longtics;
int displayplayer;
int gametic;
int basetic;
int palette_ondamage;
int palette_onbonus;
int palette_onpowers;

int xUseSuccess;
int xDoneDamage;
int xLineId;
int xLineCrossed;
int xLineVertexes[2][2];
} //extern "C"


/*
 * xdre private stuff
 */
constexpr double X_PI = 3.14159265358979323846;

unsigned char* header;
std::size_t headerSize;
std::vector<ticcmd_t> tics[4];
std::vector<ticcmd_t>::iterator nextTic[4]; //I had a list before realising I may need random access
int savepointStyle;

void display();
bool loadReplay(unsigned char const* demo_p, std::size_t size);
unsigned char const* readTics(unsigned char const* start_p, std::size_t size);
void makeCmds();
void goForward(int numTics);
void goBack(int numTics);

void display() {
    D_Display();
}

bool loadReplay(unsigned char const* demo_p, std::size_t size) {
    unsigned char const* tics_p = G_ReadDemoHeader(demo_p, size);
    if (!tics_p ) {
        return false;
    }

    if (headerSize != (tics_p - demo_p)) {
        delete[] header;
        headerSize = tics_p - demo_p;
        header = new unsigned char [headerSize];
    }

    std::memcpy(header, demo_p, headerSize);

    x_clearMapSavepoints();
    x_clearUserSavepoint();
    for (int p = 0; p < 4; ++p) {
        tics[p].clear();
    }

    unsigned char const* demomarker_p = readTics(tics_p, size - (tics_p - demo_p));
    if (!demomarker_p) {
//        return false; //TODO: maybe we don't care if it's illformed
    }
    //TODO: there might be demofooter, don't care now

    for (int p = 0; p < 4; ++p) {
        nextTic[p] = std::begin(tics[p]);
    }
    gametic = basetic = 0;
    displayplayer = consoleplayer;

    goForward(1);
    display();

    return true;
}

//modified from G_ReadOneTick
//return pointer to DEMOMARKER=0x80 or nullptr if no DEMOMARKER
unsigned char const* readTics(unsigned char const* start_p, std::size_t size) {
    unsigned char const* tic_p = start_p;
    int plr = 0;
    //maybe there is no green? dunno if possible
    for (int i = 0; i < 4; ++i) {
        if (playeringameGet(i)) {
            plr = i;
            break;
        }
    }

    while (*tic_p != 0x80 && tic_p - start_p < size) {
        ticcmd_t cmd = {};
        unsigned char at = 0;
        cmd.forwardmove = static_cast<signed char>(*tic_p++);
        cmd.sidemove = static_cast<signed char>(*tic_p++);
        //there's no real support for longtics, but this shouldn't hurt
        if (!longtics) {
            cmd.angleturn = (at = *tic_p++) << 8;
        } else {
            unsigned int lowbyte = *tic_p++;
            cmd.angleturn = ((static_cast<signed int>(*tic_p++)) << 8) + lowbyte;
        }
        cmd.buttons = *tic_p++;

        if (compatibility_level == tasdoom_compatibility) {
            signed char tmp = cmd.forwardmove;
            cmd.forwardmove = cmd.sidemove;
            cmd.sidemove = static_cast<signed char>(at);
            cmd.angleturn = cmd.buttons << 8;
            cmd.buttons = tmp;
        }

        tics[plr++].push_back(cmd);
        while (!playeringameGet(plr)) {
            if (plr >= 3) {
                plr = 0;
            } else {
                ++plr;
            }
        }
    }

    return *tic_p == 0x80 ? tic_p : nullptr;
}

void makeCmds() {
    for (int p = 0; p < 4; ++p) {
        if (!playeringameGet(p)) continue;
        if (nextTic[p] != std::end(tics[p])) {
            netcmds[p] = *nextTic[p]++;
        } else {
            netcmds[p] = {};
        }
    }
}

void goForward(int numTics) {
    if (numTics < 1) {
      return;
    }

    auto endCmd = std::end(tics[displayplayer]);
    auto dest = numTics + gametic;
    auto savepoint = x_getSavepointTic(gametic);
    auto doSaveTic = - 1;
    if (dest - savepoint > 140) {
        doSaveTic = dest - 70;
        if (doSaveTic < gametic) {
            doSaveTic = gametic;
        }
    }

    while (numTics-- > 0 && nextTic[displayplayer] != endCmd) {
        xUseSuccess = 0;
        xLineCrossed = 0;
        xDoneDamage = 0;

        if (savepointStyle == SpStyle::SP_AUTO
                && gametic > 1
                && savepoint != gametic
                && (leveltime == 1
                    || doSaveTic == gametic)) {
            x_setSavepoint(0);
        } else if (savepointStyle == SpStyle::SP_START
                && gametic > 1
                && savepoint != gametic
                && leveltime == 1) {
            x_setSavepoint(0);
        }

        makeCmds();
        G_Ticker();
        ++gametic;
    }

    xdre::setLinedefCheck(-1);
}

//if numTics = 0, refreshes currenttic
void goBack(int numTics) {
    if (numTics < 0) {
        return;
    }

    auto oldTic = gametic;
    auto destTic = gametic - numTics;
    if (destTic < 1) {
        destTic = 1;
    }

    if (savepointStyle == SpStyle::SP_USER) {
        auto temp = x_getSavepointTic(gametic);
        destTic = destTic <= temp ? temp + 1 : destTic;
    }

    if (!x_loadSavepoint(destTic)) {
        for (int p = 0; p < 4; ++p) {
            std::advance(nextTic[p], gametic - oldTic); //gametic is now the savepoint tic
        }
        goForward(destTic - gametic);
    } else {
        x_clearMapSavepoints();
        x_clearUserSavepoint();
        gametic = 0;
        G_ReadDemoHeader(header, headerSize);
        for (int p = 0; p < 4; ++p) {
            nextTic[p] = std::begin(tics[p]);
        }
        goForward(destTic);
    }
}

bool bruteCheckDone(BruteCheck const& check) {
    if (!check.rngFunc(x_getRngIndex())) {
        return false;
    }

    if (!check.damageFunc(xDoneDamage)) {
        return false;
    }

    if (!check.xPosFunc(xdre::getXPos())) {
        return false;
    }

    if (!check.yPosFunc(xdre::getYPos())) {
        return false;
    }

    if (!check.xMomFunc(xdre::getXMom())) {
        return false;
    }

    if (!check.yMomFunc(xdre::getYMom())) {
        return false;
    }

    if (!check.speedFunc(xdre::getDistanceMoved())) {
        return false;
    }

    if (check.use && !xdre::getUseSuccess()) {
        return false;
    }

    return true;
}

//recursive shit, will make you cry
bool bruteForceTics(std::vector<BruteTic>::const_iterator brute_it,
                    std::vector<BruteTic>::const_iterator bruteEnd_it,
                    BruteCheck const& bruteCheck, bool const& abort) {
    auto bruteForward = [](int numTics) {
        if (numTics < 1) {
            return;
        }

        while (numTics-- > 0) {
            xUseSuccess = 0;
            xLineCrossed = 0;
            xDoneDamage = 0;

            makeCmds();
            G_Ticker();
            ++gametic;
        }
    };

    int gameticOnEnter = gametic;
    auto bruteLoad = [gameticOnEnter, bruteForward]() {
        if (gametic == gameticOnEnter) {
            return;
        }
        int oldTic = gametic;
        if (!x_loadSavepoint(gametic + 1)) {
            std::advance(nextTic[displayplayer], gametic - oldTic);
            bruteForward(gameticOnEnter - gametic);
        } else {
            gametic = 0;
            G_ReadDemoHeader(header, headerSize);
            for (int p = 0; p < 4; ++p) {
                nextTic[p] = std::begin(tics[p]);
            }
        }
    };

    auto const& bruteTic = *brute_it;

    for (auto turn : bruteTic.turnCmds) {
        if (bruteTic.turnsAreAngles) {
            nextTic[displayplayer]->angleturn = (xdre::getAngle() - turn) << 8;
        } else {
            nextTic[displayplayer]->angleturn = turn << 8;
        }

        for (auto run : bruteTic.runCmds) {
            nextTic[displayplayer]->forwardmove = run;

            for (auto strafe : bruteTic.strafeCmds) {
                nextTic[displayplayer]->sidemove = strafe;

                nextTic[displayplayer]->buttons &= ~(BT_ATTACK|BT_USE);

                //wtf, close your eyes
                bool fire[2];
                int fireLen;
                if (bruteTic.fire < 0) { //no
                    fireLen = 1;
                    fire[0] = false;
                } else if (bruteTic.fire > 0) { //yes
                    fireLen = 1;
                    fire[0] = true;
                } else { //both
                    fireLen = 2;
                    fire[0] = false;
                    fire[1] = true;
                }

                for (int f = 0; f < fireLen; ++f) {
                    if (fire[f]) {
                        nextTic[displayplayer]->buttons |= BT_ATTACK;
                    } else {
                        nextTic[displayplayer]->buttons &= ~BT_ATTACK;
                    }

                    bool use[2];
                    int useLen;
                    if (bruteTic.use < 0) { //no
                        useLen = 1;
                        use[0] = false;
                    } else if (bruteTic.use > 0) { //yes
                        useLen = 1;
                        use[0] = true;
                    } else { //both
                        useLen = 2;
                        use[0] = false;
                        use[1] = true;
                    }

                    for (int u = 0; u < useLen; ++u) {
                        if (use[u]) {
                            nextTic[displayplayer]->buttons |= BT_USE;
                        } else {
                            nextTic[displayplayer]->buttons &= ~BT_USE;
                        }

                        //ok all tics are set
                        auto nextBrute_it = std::next(brute_it);
                        if (nextBrute_it != bruteEnd_it && nextBrute_it->tic <= bruteCheck.tic) {
                            bruteForward(nextBrute_it->tic - gametic);
                            if (bruteForceTics(nextBrute_it, bruteEnd_it, bruteCheck, abort)) {
                                return true;
                            } else if (abort) {
                                return false;
                            }
                        } else {
                            bruteForward(bruteCheck.tic - gametic + 1);
                            if (bruteCheckDone(bruteCheck)) {
                                return true;
                            } else if (abort) {
                                return false;
                            }
                        }
                        bruteLoad();
                    } //use
                } //fire
            } //strafe
        } //run
    } //turn

    return false;
}

} //namespace

//Funcion que carga una demo a partir de una ruta
void cargaDemo(const char* ruta){
    FILE* demo;
    demo = fopen(ruta, "r");
    xdre::load(demo);
}
//Funcion que saca la lista de instrucciones de una demo a partir de su ruta
std::vector<std::string> listaDesdeDemo(const char* ruta){
    using namespace std;
    vector<string> lista;
    int i;
    string espacio = " ";

    //usa la funcion auxiliar para cargar la demo
    cargaDemo(ruta);

    //mete las instrucciones de la demo en una lista de string
    //(con un espacio detras para facilitar el uso de la funcion "demoDesdeLista")
    int sizeDemo = xdre::getTiclistSize();
    for(i=0;i<sizeDemo;i++){
        lista.push_back(xdre::getTicString(i).c_str()+espacio);
    }

    return lista;
}

//Funcion que crea una lista de instrucciones a partir de otras
std::vector<std::string> sumaDeInstrucciones(std::vector<std::string> lista1, std::vector<std::string> lista2){
    std::vector<std::string> listaAgregada;
    listaAgregada = lista1;
    listaAgregada.insert(listaAgregada.end(),lista2.begin(), lista2.end());
    return listaAgregada;
}


//variable que sirve para que no se haga display en cada instrucci??n si se est?? creando una demo
bool creandoDemo = false;

//Funcion auxiliar para crear la demo con las instrucciones de la lista
ResultDemo demoDesdeLista(std::vector<std::string> list){
    //se pone a true la variable para que no haga display cada instrucci??n
    creandoDemo = true;
    ResultDemo resultado;
    int i;
    int a;
    float tiempo = 100.0;
    bool completaNivel = false;
    using namespace std;
    string tic = "WT";
    string tic2 = "WT";
    string espacio = " ";
    for (i=0;i<list.size();i++){
        tic = list[i];

        //comprueba si se mueve hacia delante
        size_t existe = tic.find("MF");
        if(existe != string::npos){
            for (a=0;a<101;a++){
                tic2="MF"+to_string(a)+espacio;
                if(tic.find(tic2)!=string::npos){
                    xdre::run(a);
                    break;
                }
            }
        }

        //comprueba si se mueve hacia atras
        existe = tic.find("MB");
        if(existe != string::npos){
            for (a=0;a<101;a++){
                tic2="MB"+to_string(a)+espacio;
                if(tic.find(tic2)!=string::npos){
                    xdre::run(-a);
                    break;
                }
            }
        }

        //comprueba si se mueve en diagonal hacia la derecha
        existe = tic.find("SR");
        if(existe != string::npos){
            for (a=0;a<101;a++){
                tic2="SR"+to_string(a)+espacio;
                if(tic.find(tic2)!=string::npos){
                    xdre::strafe(a);
                    break;
                }
            }
        }

        //comprueba si se mueve en diagonal hacia la izquierda
        existe = tic.find("SL");
        if(existe != string::npos){
            for (a=0;a<101;a++){
                tic2="SL"+to_string(a)+espacio;
                if(tic.find(tic2)!=string::npos){
                    xdre::strafe(-a);
                    break;
                }
            }
        }

        //comprueba si se gira hacia la izquierda
        existe = tic.find("TL");
        if(existe != string::npos){
            for (a=8;a<360;a++){
                tic2="TL"+to_string(a)+espacio;
                if(tic.find(tic2)!=string::npos){
                    xdre::turn(a);
                    break;
                }
            }
        }

        //comprueba si se gira hacia la derecha
        existe = tic.find("TR");
        if(existe != string::npos){
            for (a=8;a<360;a++){
                tic2="TR"+to_string(a)+espacio;
                if(tic.find(tic2)!=string::npos){
                    xdre::turn(-a);
                    break;
                }
            }
        }

        //comprueba si interactua
        existe = tic.find("U");
        if(existe != string::npos){
            xdre::toggleUse();
        }
        
        //si es el ??ltimo tic, saca el tiempo y no hagas nuevos tics
        if (i==list.size()-1){
            tiempo = xdre::getTime();
            //si Distance from Line es -3680.0, indica que ha terminado el nivel
            if(xdre::getDistanceFromLine()==-3680.0){
                completaNivel = true;
            }
        }else{
            xdre::newTic();
        }
        

    }
    //al terminar pone a false la variable
    creandoDemo = false;
    resultado.tiempo = tiempo;
    resultado.completaNivel = completaNivel;
    return resultado;
}


/*
 * xdre public
 */



//variables globales para las funciones del genetico
std::vector<std::string> demo1;
std::vector<std::string> demo2;
std::vector<std::string> listaTotal;

//Modifica una instrucci??n aleatoria de un candidato (demo) usando una de la lista de instrucciones
void mutate(std::vector<std::string>& candidate) {
  int pivot = rand() % candidate.size();
  int pivotListaTotal = rand() % listaTotal.size();
  candidate[pivot] = listaTotal[pivotListaTotal];
}

int cuentaCandidatos = 0;

int fitness(const std::vector<std::string>& candidate) {
  int R = 500000;
  //Si la demo no completa el nivel, aumenta el fitness en 10000
  // en caso de que lo complete, disminuye el fitness seg??n el tiempo tardado (igual hay que multiplicarlo por 100 o algo as??)
  using namespace std;
  ResultDemo resultado;
  float tiempo = 100.0;
  bool completaNivel = false;
  resultado = demoDesdeLista(candidate);

  tiempo = resultado.tiempo;
  completaNivel = resultado.completaNivel;

  //si no completa el nivel, le resta 300000
  if(completaNivel == false){
    R += -300000;
  //si lo completa, le resta el tiempo * 100
  }
    R += -tiempo*100;
    cuentaCandidatos+=1;
  return R;
}


//Devuelve un vector con 2 posiciones (los dos hijos) al mezclar dos demos candidatas
std::vector<std::vector<std::string>> cross(const std::vector<std::string>& p1, const std::vector<std::string>& p2) {
  std::vector<std::vector<std::string>> children(2);
  int pivot = rand() % (p1.size()-1);
  //A??ade a cada uno las "pivot" primeras instrucciones de cada demo
  for(int i=0;i<pivot;i++){
    children[0].push_back(p1[i]);
    children[1].push_back(p2[i]);
  }
  //A??ade a cada uno las "pivot" siguientes instrucciones de la otra demo
  for (int i=pivot;i<p2.size();i++){
    children[0].push_back(p2[i]);
  }
  for (int i=pivot;i<p1.size();i++){
    children[1].push_back(p1[i]);
  }
  return children;
}

//Crea el primer candidato con tama??o 350 tics de instrucciones aleatorias de la lista de instrucciones
std::vector<std::string> random_demo(){
  std::vector<std::string> nueva_demo;
  int size = 350;
  for (int i=0;i<size;i++){
    int pivot = rand() % listaTotal.size();
    nueva_demo.push_back(listaTotal[pivot]);
  }
  return nueva_demo;
}


//variable para guardar el resultado del algoritmo
std::vector<std::string> resultadoGenetico;


int mainGenetico() {
  genetic_algorithm<std::vector<std::string>> genetico_TAS(10, 0.7, 0.2, &random_demo, &cross, &fitness, &mutate);
  std::vector<std::string> best = genetico_TAS.best_candidate();
  //si vale 200000 es que no ha terminado el nivel (500000 - 300000 de penalizaci??n)
  while (fitness(best)<200001) {
      //paro el algoritmo al llegar a 100 candidatos, 
      if(cuentaCandidatos>=100){
          printf("Ha llegado a 100 candidatos\n");
          break;
      }
    genetico_TAS.do_generation();
    best = genetico_TAS.best_candidate();
    
  }
  std::cout << "generations: " << genetico_TAS.get_generation_count() << std::endl;

  //guarda en la variable el mejor candidato obtenido
  resultadoGenetico = best;

  return 0;
}

//funcion para ejecutar
std::vector<std::string> obtieneResultadoGenetico(){
  mainGenetico();
  return resultadoGenetico;
}



void xdre::init() {
    printf("----- INICIANDO APLICACION Y ALGORITMO GENETICO -----\n");
    static bool done;
    if (done) {
        return;
    }

    initDoom(mainArgc, mainArgv);

    tics[0].resize(10, {});
    nextTic[0] = std::begin(tics[0]);
    for (int p = 1; p < 4; ++p) {
        tics[p].clear();
        nextTic[p] = std::end(tics[p]);
    }

    headerSize = 13;
    header = new unsigned char [13];
    x_initHeader(header);

    palette_onbonus = 1;
    palette_ondamage = 1;
    palette_onpowers = 1;

    savepointStyle = SpStyle::SP_USER;

    //para que no haga display
    creandoDemo=true;
    cuentaCandidatos = 0;
    goForward(1);

    using namespace std;

    FILE* demoVacia;
    demoVacia = fopen("../DEMOS/demoBase.lmp", "r");
    //demoVacia = fopen("/media/sf_Compartida_Ubuntu/DEMOS/demoBase.lmp", "r");
    load(demoVacia);

    ResultDemo resultado;
    float tiempo = 100.0;
    bool completaNivel = false;
    
    //Estas ruta se ha cambiado al subirlo a GitHub, las que se han comentado son las originales usadas para sacar los resultados
    // en la m??quina virtual
    demo1 = listaDesdeDemo("../DEMOS/pruebaTAS11-03parte1v4.lmp")
    demo2 = listaDesdeDemo("../DEMOS/pruebaTAS11-03parte2.lmp");
    //demo1 = listaDesdeDemo("/home/alf/Documents/DEMOS/pruebaTAS11-03parte1v4.lmp");
    //demo2 = listaDesdeDemo("/home/alf/Documents/DEMOS/pruebaTAS11-03parte2.lmp");
    listaTotal = sumaDeInstrucciones(demo1, demo2);
    vector<string> listaGenetico = obtieneResultadoGenetico();
    resultado = demoDesdeLista(listaGenetico);
    

    tiempo = resultado.tiempo;
    completaNivel = resultado.completaNivel;

    cout << "Tiempo: " << tiempo << endl;
    if(completaNivel == false){
        cout << "Completa el nivel: No" << endl;
    }else{
        cout << "Completa el nivel: S??" << endl;
    }

    display();
    done = true;
}



bool xdre::bruteForce(std::vector<BruteTic> bruteTics, BruteCheck const& bruteCheck, bool const& abort) {
    std::sort(std::begin(bruteTics), std::end(bruteTics), [](BruteTic const& lhs, BruteTic const& rhs){return lhs.tic < rhs.tic;});
    int bruteStart = bruteTics.front().tic;
    int bruteEnd = bruteTics.back().tic;
    if (bruteStart < 0 || tics[displayplayer].size() - 1 < bruteEnd || bruteCheck.tic < bruteStart) {
        return false;
    }

    x_clearMapSavepoints();
    x_clearUserSavepoint();

    gametic = 0;
    G_ReadDemoHeader(header, headerSize);
    for (int p = 0; p < 4; ++p) {
        nextTic[p] = std::begin(tics[p]);
    }

    int destSavepoint = bruteStart;
    while (destSavepoint-- > 0)
    {
        makeCmds();
        G_Ticker();
        ++gametic;
    }

    if (gametic > 0) {
        if (gamestate == GS_LEVEL) {
            x_setSavepoint(1);
        } else {
        //fuck it
        }
    }

    xUseSuccess = 0;
    xLineCrossed = 0;
    xDoneDamage = 0;

    //here we go
    if (bruteForceTics(bruteTics.cbegin(), bruteTics.cend(), bruteCheck, abort)) {
        goBack(0);
        display();
        return true;
    }

    if (abort) {
        x_clearMapSavepoints();
        x_clearUserSavepoint();
        goBack(0);
        goForward(1);
    } else {
        goForward(1);
        goBack(0);
    }

    display();
    return false;
}

void xdre::doSdlEvents() {
    I_StartTic();
}

bool xdre::save(std::FILE* file) {
    rewind(file);

    fwrite(header, sizeof header[0], headerSize, file);

    decltype(std::begin(tics[0])) iters[4] {
        std::begin(tics[0]),
        std::begin(tics[1]),
        std::begin(tics[2]),
        std::begin(tics[3]),
    };

    decltype(std::end(tics[0])) ends[4] {
        std::end(tics[0]),
        std::end(tics[1]),
        std::end(tics[2]),
        std::end(tics[3]),
    };

    ticcmd_t nulltic {};
    while (iters[0] != ends[0] ||
           iters[1] != ends[1] ||
           iters[2] != ends[2] ||
           iters[3] != ends[3]) {
        for (int p = 0; p < 4; ++p) {
            if (iters[p] != ends[p]) {
                G_WriteDemoTiccmd(&*iters[p], file);
                ++iters[p];
            } else if (playeringameGet(p)) {
                G_WriteDemoTiccmd(&nulltic, file);
            }
        }
    }

    fputc(0x80, file);

    return true;
}

bool xdre::load(std::FILE* file) {
    std::fseek(file, 0, SEEK_END);
    auto size = std::ftell(file);
    rewind(file);
    auto data = new unsigned char[size];
    std::fread(&data[0], sizeof data[0], size, file);

    bool ret = loadReplay(data, size);

    delete[] data;
    return ret;
}

void xdre::setSavepoint() {
    if (!x_setSavepoint(1)) {
        goForward(1);
        display();
    }
}

void xdre::unsetSavepoint() {
    x_clearUserSavepoint();
    if (savepointStyle == SpStyle::SP_AUTO) {
        savepointStyle = SpStyle::SP_START;
    }

    goBack(0);
    display();
}

void xdre::seekDemo(int numTics) {
    if (numTics < 0) {
        goBack(-numTics);
    } else {
        goForward(numTics);
    }

    display();
}

void xdre::copyTic() {
    auto tic = std::prev(nextTic[displayplayer]);
    if (tic == std::end(tics[displayplayer])) {
        return;
    }

    tics[displayplayer].emplace(nextTic[displayplayer], *tic);
    goForward(1);
    //comprueba que no se est?? creando una demo
    if(creandoDemo==false){
        display();
    }
}

void xdre::deleteTic() {
    auto tic = std::prev(nextTic[displayplayer]);
    if (tic == std::end(tics[displayplayer])) {
        return;
    }

    tics[displayplayer].erase(tic);
    goBack(0);
    //comprueba que no se est?? creando una demo
    if(creandoDemo==false){
        display();
    }
}

void xdre::newTic() {
    tics[displayplayer].emplace(nextTic[displayplayer], ticcmd_t{});
    goForward(1);
    //comprueba que no se est?? creando una demo
    if(creandoDemo==false){
        display();
    }
}

void xdre::changePlayerView(unsigned int plr) {
    if (plr < 1 || plr > 4) {
        return;
    } else if (!playeringameGet(plr - 1)) {
        return;
    } else if (displayplayer == plr - 1) {
        return;
    } else if (gamestate != GS_LEVEL) {
        return;
    }

    displayplayer = plr - 1;

    ST_Start();
    HU_Start();
    display();
}

void xdre::toggleBlood() {
    palette_onbonus ^= 1;
    palette_ondamage ^= 1;
    palette_onpowers ^= 1;

    ST_Start();
    HU_Start();
    display();
}

void xdre::toggleFire() {
    std::prev(nextTic[displayplayer])->buttons ^= BT_ATTACK;
    goBack(0);
    //comprueba que no se est?? creando una demo
    if(creandoDemo==false){
        display();
    }
}

void xdre::toggleUse() {
    std::prev(nextTic[displayplayer])->buttons ^= BT_USE;
    goBack(0);
    //comprueba que no se est?? creando una demo
    if(creandoDemo==false){
        display();
    }
}

void xdre::changeWeapon(char wpn) {
    auto tic = std::prev(nextTic[displayplayer]);
    tic->buttons &= ~(BT_CHANGE | BT_WEAPONMASK);

    //adjust wpn < 9 if game has more guns
    if (wpn > 0 && wpn < 9) {
        tic->buttons |= BT_CHANGE;
        tic->buttons |= (wpn - 1) << BT_WEAPONSHIFT;
    }

    goBack(0);
    //comprueba que no se est?? creando una demo
    if(creandoDemo==false){
        display();
    }
}

void xdre::run(signed char val) {
    std::prev(nextTic[displayplayer])->forwardmove = val;
    goBack(0);
    //comprueba que no se est?? creando una demo
    if(creandoDemo==false){
        display();
    }
}

void xdre::strafe(signed char val) {
    std::prev(nextTic[displayplayer])->sidemove = val;
    goBack(0);
    //comprueba que no se est?? creando una demo
    if(creandoDemo==false){
        display();
    }
}

void xdre::turn(signed char val) {
    std::prev(nextTic[displayplayer])->angleturn = val << 8;
    goBack(0);
    //comprueba que no se est?? creando una demo
    if(creandoDemo==false){
        display();
    }
}

void xdre::turnStepLeft() {
    std::prev(nextTic[displayplayer])->angleturn += 1 << 8;
    goBack(0);
    //comprueba que no se est?? creando una demo
    if(creandoDemo==false){
        display();
    }
}

void xdre::turnStepRight() {
    std::prev(nextTic[displayplayer])->angleturn -= 1 << 8;
    goBack(0);
    //comprueba que no se est?? creando una demo
    if(creandoDemo==false){
        display();
    }
}

void xdre::setSavepointStyle(int style) {
    if (savepointStyle == style) {
        return;
    }

    savepointStyle = style;
    if (savepointStyle == SpStyle::SP_USER) {
        x_clearMapSavepoints();
    }
}

void xdre::setLinedefCheck(int linedef) {
    x_setLinedefCheck(linedef);
}

std::string xdre::getTicString(unsigned int n) {
    if (n >= tics[displayplayer].size()) {
        return "";
    }

    std::stringstream ss;
    ticcmd_t const& tic = tics[displayplayer][n];

    if (tic.forwardmove > 0) {
        ss << " MF" << +tic.forwardmove;
    } else if (tic.forwardmove < 0) {
        ss << " MB" << -tic.forwardmove;
    }

    if (tic.sidemove > 0) {
        ss << " SR" << +tic.sidemove;
    } else if (tic.sidemove < 0) {
        ss << " SL" << -tic.sidemove;
    }

    if (tic.angleturn > 0) {
        ss << " TL" << (tic.angleturn >> 8);
    } else if (tic.angleturn < 0) {
        ss << " TR" << -(tic.angleturn >> 8);
    }

    if (tic.buttons & BT_ATTACK) {
        ss << " F";
    }

    if (tic.buttons & BT_USE) {
        ss << " U";
    }

    if (tic.buttons & BT_CHANGE) {
        ss << " G" << (((tic.buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT) + 1);
    }

    std::string str = ss.str();
    return str.empty() ? " WT" : str;
}

unsigned int xdre::getTiclistSize() {
    return tics[displayplayer].size();
}

int xdre::getRngIndex() {
    return x_getRngIndex();
}

double xdre::getXPos() {
    return x_getXPos();
}

double xdre::getYPos() {
    return x_getYPos();
}

double xdre::getZPos() {
    return x_getZPos();
}

double xdre::getXMom() {
    return x_getXMom();
}

double xdre::getYMom() {
    return x_getYMom();
}

double xdre::getZMom() {
    return x_getZMom();
}

double xdre::getDistanceMoved() {
    auto dx = x_getXPos() - x_getPrevXPos();
    auto dy = x_getYPos() - x_getPrevYPos();
    return std::sqrt(dx * dx + dy * dy);
}

double xdre::getDirectionMoved() {
    auto dx = x_getXPos() - x_getPrevXPos();
    auto dy = x_getYPos() - x_getPrevYPos();
    auto angle = std::atan(dx / dy) * 128 / X_PI + 64;
    angle = dy < 0 ? angle - 128 : angle;
    return std::isnan(angle) ? 0 : angle;
}

signed char xdre::getAngle() {
    return x_getAngle();
}

bool xdre::getUseSuccess() {
    if (xUseSuccess == 0) {
        return false;
    }

    return true;
}

bool xdre::getLinedefCrossed() {
    if (xLineCrossed == 0) {
        return false;
    }

    return true;
}

double xdre::getLinedefVertex(int n, int xy) {
    if (xLineId == -1 || n < 0 || n > 1 || xy < 0 || xy > 1) {
        return 0;
    }

    return xLineVertexes[n][xy] / static_cast<double>(0x10000);
}

double xdre::getDistanceFromLine() {
    if (xLineId == -1) {
        return 0;
    }

    struct {
        double x;
        double y;
    } PA, AB, PX;

    double dist;
    double t;
    double v1x = getLinedefVertex(0, 0);
    double v1y = getLinedefVertex(0, 1);
    double v2x = getLinedefVertex(1, 0);
    double v2y = getLinedefVertex(1, 1);
    // dist = |PX| = |PA + tAB| = |PA + AB * (-PA.AB) / |AB|^2|
    PA.x = v1x - x_getXPos();
    PA.y = v1y - x_getYPos();
    AB.x = v2x - v1x;
    AB.y = v2y - v1y;

    t = -PA.x * AB.x + -PA.y * AB.y;
    t /= AB.x * AB.x + AB.y * AB.y;

    PX.x = PA.x + t * AB.x;
    PX.y = PA.y + t * AB.y;

    dist = std::sqrt(PX.x * PX.x + PX.y * PX.y);
    if (((v2y - v1y) / (v2x - v1x)) * (x_getXPos() - v1x) + v1y < x_getYPos()) {
        dist *= -1;
    }

    return dist;
}

int xdre::getDoneDamage() {
    return xDoneDamage;
}

float xdre::getTime() {
    return leveltime / 35.0;
}

unsigned int xdre::getCurrentTic() {
    return gametic - 1;
}

unsigned int xdre::getSavepointTic() {
    return x_getSavepointTic(gametic);
}

int xdre::getSavepointStyle() {
    return savepointStyle;
}

bool xdre::getBlood() {
    if (palette_ondamage != 0) {
        return true;
    }

    return false;
}
