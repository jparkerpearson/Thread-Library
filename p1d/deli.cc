// created by parker
//
//
#include <string>
#include "thread.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <vector>
using namespace std;






// functions
void topThread(void*);
void cashThread(void*);
void makerThread();


unsigned int lock = 123;
unsigned int newSandwichOnBoard = 9872;
std::vector<pair<int,int> > orderBoard;
std::vector<pair<int ,bool> > cashiers;
int lastSandwich=-1;
int currentSandwich;
int numCashiers;
int maxBoard;
int cashId;


int main(int argc, char *argv[]){
    maxBoard = atoi(argv[1]);  	// maxOrders on the orderBoard
    numCashiers = argc-2;
    if(numCashiers<maxBoard){
        maxBoard = numCashiers;
    }
    thread_libinit((thread_startfunc_t)topThread, argv);
}






void makerThread(){
    thread_lock(lock);
    int id;
    while(numCashiers!=0){
        
        // only make sandwiches when board is full
        while(orderBoard.size()!=maxBoard){
            
            
            thread_wait(lock, newSandwichOnBoard);
        }
        
        // board is full so find sandwich which is most similair to last sandwich
        int closest=1001;
        int numInBoard;
        
        int readyCashier;
        for (int i=0; i< orderBoard.size();i++) {
            if((abs(orderBoard[i].first-lastSandwich)) <  closest) { // closer sando
                numInBoard=i;
                closest=abs(orderBoard[i].first-lastSandwich);
                currentSandwich=orderBoard[i].first;
                readyCashier=orderBoard[i].second;
            }
        }
        for (int i=0; i< cashiers.size(); i++ ) { // mark cashier as free
            if (cashiers[i].first == readyCashier ) {
                cashiers[i].second=true;
                break;
            }
        }
        orderBoard.erase(orderBoard.begin()+numInBoard);
        lastSandwich=currentSandwich;
        
        
        
        cout << "READY: cashier " << readyCashier << " sandwich " << currentSandwich << endl;
        // signal to cashier who requested that sandwich is ready
        
        for( int i=0; i <cashiers.size();i++) {
            if( cashiers[i].second==true) { // if waiting
                
                thread_signal(lock, cashiers[i].first);
            }
            
        }
    }
    thread_unlock(lock);
}



void topThread(void* files){
    void* currentInput;
    
    char** inputList=   (char**)files;
    
    
    for (int i=0; i<numCashiers; i++){  // cashier threads
        currentInput = ((void*)(inputList[i+2]));
        thread_create((thread_startfunc_t)cashThread, currentInput);
    }
    
    thread_create((thread_startfunc_t)makerThread, 0); //
    
    
    
}




void cashThread(void* inputFile){
    
    thread_lock(lock);
    
    int cashierId = cashId;
    cashId=cashId+1;
    cashiers.push_back(std::make_pair(cashierId,true));
    char* inFile = (char*)inputFile;
    std::ifstream sandwichRead(inFile,std::ifstream::in);
    string curOrder;
    int orderNum;
    while(getline(sandwichRead, curOrder)){
       
        
        orderNum = atoi(  curOrder.c_str()  );
        while (orderBoard.size()==maxBoard) {
           
            thread_wait(lock, cashierId);
            
        }
        
        orderBoard.push_back(std::make_pair(orderNum,cashierId));
        for (int i=0; i<cashiers.size();i++) {
            if (cashiers[i].first == cashierId) {
                cashiers[i].second=false;
                break;
            }
        }
        // print orders been placed
        cout << "POSTED: cashier " << cashierId << " sandwich " << orderNum << endl;
        thread_signal(lock, newSandwichOnBoard);
        
        for( int i=0; i<cashiers.size(); i++) {
            if (cashiers[i].first == cashierId) {
                while (!cashiers[i].second) {
                    thread_wait(lock,cashierId);
                }
                break;
            }
        }
        
    }
    sandwichRead.close(); //	stop taking inputs
    numCashiers--; // keep track of number of cashiers
    
    
    if(     numCashiers< maxBoard){
        maxBoard = numCashiers;
        if(numCashiers !=0){
            thread_signal(lock, newSandwichOnBoard);
        }
    }
    
    
    thread_unlock(lock);
}






