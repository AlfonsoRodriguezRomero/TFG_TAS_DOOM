/*
  MIT License

  Copyright (c) 2017 Coen Valk
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "genetic-algorithm.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <bitset>
#include <string>
#include <cassert>

#include "../../xdre.hpp"

using namespace std;
vector<string> demo1 = listaDesdeDemo("/media/sf_Compartida_Ubuntu/DEMOS/pruebaTAS11-03parte1v4.lmp");
vector<string> demo2 = listaDesdeDemo("/media/sf_Compartida_Ubuntu/DEMOS/pruebaTAS11-03parte2.lmp");
vector<string> listaTotal = sumaDeInstrucciones(demo1, demo2);

//Modifica una instrucción aleatoria de un candidato (demo) usando una de la lista de instrucciones
void mutate(std::vector<std::string>& candidate) {
  int pivot = rand() % candidate.size();
  int pivotListaTotal = rand() % listaTotal.size();
  candidate[pivot] = listaTotal[pivotListaTotal];
}

void mutate0(int& candidate) {
  int pivot = rand() % 32;
  candidate = candidate ^ (1 << pivot);
}

void mutate1(std::string& candidate) {
  int pivot = rand() % candidate.length();
  candidate[pivot] = (char) ((rand() % 96) + 32);
}

int fitness(const std::vector<std::string>& candidate) {
  int R = 500000;
  //Si la demo no completa el nivel, aumenta el fitness en 10000
  // en caso de que lo complete, disminuye el fitness según el tiempo tardado (igual hay que multiplicarlo por 100 o algo así)
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
  }else{
    R += -tiempo*100;
  }

  return R;
}

int fitness0(const int& candidate) {
  int R = 0;
  for (int i = 0; i < 32; i++) {
    // isolate:
    R += (candidate >> i) & 1;
  }
  return R;
}

int fitness1(const std::string& candidate) {
  std::string test_string = "The quick brown fox jumps over the lazy dog.";
  int R = 0;
  for (int i = 0; i < candidate.size(); i++) {
    R += 96 - std::abs(candidate[i] - test_string[i]);
  }
  return R;
}

//Devuelve un vector con 2 posiciones (los dos hijos) al mezclar dos demos candidatas
std::vector<std::vector<std::string>> cross(const std::vector<std::string>& p1, const std::vector<std::string>& p2) {
  std::vector<std::vector<std::string>> children(2);
  int pivot = rand() % p1.size();
  //Añade a cada uno las "pivot" primeras instrucciones de cada demo
  for(int i=0;i<pivot;i++){
    children[0].push_back(p1[i]);
    children[1].push_back(p2[i]);
  }
  //Añade a cada uno las "pivot" siguientes instrucciones de la otra demo
  for (int i=pivot;i<=p2.size();i++){
    children[0].push_back(p2[i]);
  }
  for (int i=pivot;i<=p1.size();i++){
    children[1].push_back(p1[i]);
  }

  return children;
}

std::vector<int> cross0(const int& p1, const int& p2) {
  std::vector<int> children(2);
  int pivot = rand() % 32;
  int bottom = (1 << pivot) - 1;
  int top = ~bottom;
  children[0] = (p1 & top) | (p2 & bottom);
  children[1] = (p1 & bottom) | (p2 & top);
  return children;
}

std::vector<std::string> cross1(const std::string& p1, const std::string& p2) {
  std::vector<std::string> children(2);
  int pivot = rand() % p1.length();
  children[0] = p1.substr(0, pivot) + p2.substr(pivot);
  children[1] = p2.substr(0, pivot) + p1.substr(pivot);
  return children;
}

//Crea el primer candidato con tamaño 350 tics de instrucciones aleatorias de la lista de instrucciones
std::vector<std::string> random_demo(){
  std::vector<std::string> nueva_demo;
  int size = 350;
  for (int i=0;i<size;i++){
    int pivot = rand() % listaTotal.size();
    nueva_demo.push_back(listaTotal[pivot]);
  }
  return nueva_demo;
}

std::string random_string() {
  int len = 44;
  std::string new_string;
  for (int i = 0; i < len; i++) {
    new_string += (char) ((rand() % 96) + 32);
  }
  return new_string;
}

//variable para guardar el resultado del algoritmo
std::vector<std::string> resultadoGenetico;


int mainGenetico() {
  // string test:
  genetic_algorithm<std::vector<std::string>> genetico_TAS(5000, 0.7, 0.2, &random_demo, &cross, &fitness, &mutate);
  std::vector<std::string> best = genetico_TAS.best_candidate();
  //si vale 200000 es que no ha terminado el nivel (500000 - 300000 de penalización)
  while (fitness(best)==200000) {
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

/**
  srand(1);
  // integer test:
  genetic_algorithm<int> test0(10, .7, 0.2, &rand, &cross0, &fitness0, &mutate0);
  int best = test0.best_candidate();
  std::string binary = std::bitset<32>(best).to_string(); //to binary
  std::cout<<binary<< ": " << fitness0(best) << std::endl;
  while (fitness0(best) < 32) {
    test0.do_generation();
    best = test0.best_candidate();
    std::string binary = std::bitset<32>(best).to_string(); //to binary
    std::cout<<binary<< ": " << fitness0(best) << std::endl;
  }
  std::cout << "generations: " << test0.get_generation_count() << std::endl;

  // string test:
  genetic_algorithm<std::string> test1(5000, 0.7, 0.2, &random_string, &cross1, &fitness1, &mutate1);
  while (test1.best_candidate() != "The quick brown fox jumps over the lazy dog.") {
    test1.do_generation();
    std::cout << test1.best_candidate() << std::endl;
  }
  std::cout << "generations: " << test1.get_generation_count() << std::endl;

**/