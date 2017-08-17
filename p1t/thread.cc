

#include <iostream>
#include <assert.h>
#include <string>
#include <stdlib.h>
#include <queue>
#include "thread.h"
#include "interrupt.h"
#include "ucontext.h"
#include <new>
#include <vector>

////////////////////////////////////////////
int initial = 0; // check to see if thread library has been initialized
// pointer to finished thread
ucontext_t* finishedThread;


std::queue <ucontext_t*> readyQ;// tracks threads that are ready
typedef std::pair<std::queue<ucontext_t*>,unsigned int> cvQ;
std::vector<cvQ> cvQVector;
std::vector<cvQ> blockedThreads;
typedef std::pair < ucontext_t*,unsigned int> holdingLock;
std::vector<holdingLock> holdLocks;
int tracker=0;
int topThread(thread_startfunc_t func, void* arg);





// tracks current thread
ucontext_t* currentThread = new ucontext_t;
//before lib is called need thread
ucontext_t* startingThread= new ucontext_t;



////////////////////////////////////////////
int thread_libinit(thread_startfunc_t func, void* arg){
    // disable check for initilization
    interrupt_disable();
    
    if (initial!=0) {
            interrupt_enable();
            return -1;
    }
    
    
    initial++; // change initial to reflect that thread library has been initialzied
    
    
    /*
     * Initialize a context structure by copying the current thread's context.
     */
    getcontext(currentThread); // ucontext_ptr has type (ucontext_t *)
    
    /*
     * Direct the new thread to use a different stack. Your thread library
     * should allocate STACK_SIZE bytes for each thread's stack.
     */
    char *stack = new char [STACK_SIZE];
    currentThread->uc_stack.ss_sp = stack;
    currentThread->uc_stack.ss_size = STACK_SIZE;
    currentThread->uc_stack.ss_flags = 0;
    currentThread->uc_link = NULL;
    
    
    
    void  (*currentFunction)();
    currentFunction = (void (*)()) & topThread;
    
    makecontext(currentThread, currentFunction, 2, func, arg);
    
    swapcontext(startingThread, currentThread);
    interrupt_enable();
    return 0;
    
}

int topThread(thread_startfunc_t func, void* arg){
    interrupt_enable();
    func(arg);
    interrupt_disable();
    tracker++;
    
    //thread is finishesd so kill
    if(finishedThread!=NULL){ // need to deleat the thread and the stack
        
        delete (char *)finishedThread->uc_stack.ss_sp;
        
        
        delete finishedThread;
    }
    
    finishedThread = currentThread;
    // check ready q
    bool empty = readyQ.empty();
    if(!empty){
        
        ucontext_t* nextThread=readyQ.front(); //next thread is front of ready q
        ucontext_t* previousCurrent = currentThread;
        currentThread = nextThread;
        readyQ.pop();
        swapcontext(previousCurrent, currentThread);
        
        interrupt_enable();
        
        return 0;
    }
    
    
    else{
        std::cout << "Thread library exiting.\n";
        interrupt_enable();
        exit(0);
        return 0;
    }
}




/*Caller releases lock and continues running.
 If the lock queue is not empty, then wake up a thread by moving it from the head of the lock queue to the tail of the ready queue: it transitions from blocked -> ready.
 
 
 
 */
int thread_unlock(unsigned int lock){
    interrupt_disable();
    
    
    
    
    // check if libinit has been called
    if (initial!=1) {
        
        interrupt_enable();
        return -1;
    }
    ucontext_t* currentLock;
    
    bool lockExists=false;
    for (int i=0; i <holdLocks.size();i++) { // release lock
        if (holdLocks[i].second == lock) {
            currentLock=holdLocks[i].first;
            if (currentLock != currentThread) { // lock not held by current thread
                interrupt_enable();
                return -1;
            }
            
            holdLocks.erase(holdLocks.begin()+i);
            lockExists=true;
        }
    }
    if (!lockExists) { // lock is already free or doesn't exsist
        interrupt_enable();
        return -1;
    }
    // if lock q not empty wake up thread by moving it to tail of ready q
    for (int i=0; i<blockedThreads.size();i++) {
        if (blockedThreads[i].second==lock && blockedThreads[i].first.size()!=0) {
            ucontext_t* moveThread;
            moveThread=blockedThreads[i].first.front();
            blockedThreads[i].first.pop();
            holdLocks.push_back(std::make_pair(moveThread,lock)); //give lock to thread at top
            readyQ.push(moveThread);
            break;
        }
    }
    //continue running
    interrupt_enable();
    return 0;
}





// create a new thread and put in onto the ready queue
int thread_create(thread_startfunc_t func, void* arg){
    interrupt_disable();
    
    // check if libinit has been called
    if (initial!=1) {
        interrupt_enable();
        return -1;
    }
    
    
    // create pointer to new context
    ucontext_t* threadPointer = new  ucontext_t;
  
    
    
    
    /*
     * Initialize a context structure by copying the current thread's context.
     */
    getcontext(threadPointer); // ucontext_ptr has type (ucontext_t *)
    
    /*
     * Direct the new thread to use a different stack. Your thread library
     * should allocate STACK_SIZE bytes for each thread's stack.
     */
    char *stack = new char [STACK_SIZE];
    
    threadPointer->uc_stack.ss_sp = stack;
    
    threadPointer->uc_stack.ss_size = STACK_SIZE;
    
    threadPointer->uc_stack.ss_flags = 0;
    
    threadPointer->uc_link = NULL;
    
    void (*currentFunction)() = (void (*)()) & topThread;
    
    makecontext(threadPointer, currentFunction, 2, func, arg);
    
    readyQ.push(threadPointer);
    
    interrupt_enable();
    return 0;
    
    
}

// Move all threads in the CV queue to ready queue
int thread_broadcast(unsigned int lock, unsigned int cond){
    interrupt_disable();
    // check if libinit has been called
    if (initial!=1) {
        interrupt_enable();
        return -1;
    }
    ///////////////////////////////
    
    
    std::queue <ucontext_t*> currentCVQ; // initialize q that stores q's waiting for specific cv
    bool foundThread= false;
    for (int i=0; i< cvQVector.size();i++) // iterate through vector of pairs
    {
        if (cvQVector[i].second == cond) { // if we find the given pair for the cv
            currentCVQ=cvQVector[i].first;
            foundThread=true;
            break;
        }
        
    }
    
    if (!foundThread) {  	    // make sure CV queue exisits
        interrupt_enable();
        return 0;
    }
    
    
    
    else{
        while (!currentCVQ.empty()) {
            
            ucontext_t* signalThread;
            signalThread = currentCVQ.front();
            currentCVQ.pop();
            readyQ.push(signalThread);
        }
    }
    interrupt_enable();
    return 0;
    
    
    
    
    
    
}


// signal top of CV queue
int thread_signal(unsigned int lock, unsigned int cond){
    interrupt_disable();
    // check if libinit has been called
    if (initial!=1) {
        interrupt_enable();
        return -1;
    }
    ///////////////////////////////
    
    
    std::queue <ucontext_t*> currentCVQ; // initialize q that stores q's waiting for specific cv
    bool foundThread= false;
    for (int i=0; i< cvQVector.size();i++) // iterate through vector of pairs
    {
        if (cvQVector[i].second == cond) { // if we find the given pair for the cv
            currentCVQ=cvQVector[i].first;
            foundThread=true;
            
            ucontext_t* signalThread;
            signalThread = cvQVector[i].first.front();
            cvQVector[i].first.pop();
            readyQ.push(signalThread);
            break;
        }
        
    }
    
    if (!foundThread) {  	// make sure CV queue exisits
        interrupt_enable();
        return 0;
    }
    
    
    
    interrupt_enable();
    return 0;
    
}





int thread_wait(unsigned int lock, unsigned int cond){
    interrupt_disable();
    
    // check if libinit has been called
    if (initial!=1) {
        interrupt_enable();
        return -1;
    }
    
    /*  Caller unlocks the lock (see thread_unlock), blocks (running blocked), and moves to tail of the CV waiter queue. Another thread from the front of the ready queue runs (ready running). Later, when the caller wakes up (e.g., due to thread_signal) and runs, it re-acquires the lock (see thread_lock) and returns. Forgetting about the lock cost the full credit for this one.
     
     */
    ucontext_t* currentLock;
    bool lockExists=false;
    for (int i=0; i <holdLocks.size();i++) {
        if (holdLocks[i].second == lock) {
            currentLock=holdLocks[i].first;
            if (currentLock != currentThread) { // lock not held by current thread
                interrupt_enable();
                return -1;
            }
            
            holdLocks.erase(holdLocks.begin()+i);
            lockExists=true;
        }
    }
    // if lock doesnt exsit
    if (!lockExists) {
        interrupt_enable();
        return -1;
    }
    
    
    bool foundThread =false;
    // move current thread to end of cvQ
    for (int i=0; i< cvQVector.size();i++) // iterate through vector of pairs
    {
        if (cvQVector[i].second == cond) { // if we find the given pair for the cv
            cvQVector[i].first.push(currentThread); // put current thread into cv
            foundThread=true;
            break;
        }
        
    }
    
    if (!foundThread) { // need to create new cv
        std::queue<ucontext_t*> currentQ;
        currentQ.push(currentThread);
        cvQVector.push_back(make_pair(currentQ,cond));
    }
    
        for (int i=0; i< blockedThreads.size();i++) { // iterate through vector of pairs
        if (blockedThreads[i].second == lock) {  // if we find the given pair for the blocked thread
            readyQ.push(blockedThreads[i].first.front() ); // move to ready q
            blockedThreads[i].first.pop(); // remove from blocked q
        }
    }
    // switch to top of ready q if ready q empty then starvation
    if (readyQ.empty()) {
        
        
        std::cout << "Thread library exiting.\n";
        interrupt_enable();
        exit(0);
        return 0;
    }
    
    
    // need to aquire the lock
    
    
    ucontext_t* nxtThread=readyQ.front();
    readyQ.pop();
    ucontext_t* previousCurrent = currentThread;
    currentThread = nxtThread;
    swapcontext(previousCurrent, currentThread);
    interrupt_enable();
    return 0;
    
}

// yields thread to top of running queue
int thread_yield(){
    interrupt_disable();
    
    if (initial!=1) {
        interrupt_enable();
        return -1;
    }
    
    // if ready queue is empty then continue without doing anything
    
    if (readyQ.empty()) {
        interrupt_enable();
        return 0;
    }
    
    
    
    ucontext_t* changeThread = readyQ.front(); // get thread at front of
    ucontext_t* hold = currentThread; // temporary save of the current thread to push it to the ready queue
    currentThread = changeThread; // change current thread to new current thread
    
    // Modify ready queue
    readyQ.pop(); // remove top of ready thread
    readyQ.push(hold);  // add current thread to the end of the ready queue
    
    
    swapcontext(hold, currentThread); // actually change the running thread
    
    
    
    interrupt_enable();
    return 0;
    
}













// pass lock
int thread_lock(unsigned int lock){
    interrupt_disable();
    
    // check if libinit has been called
    if (initial!=1) {
        interrupt_enable();
        return -1;
    }
    //check if lock is free
    bool lockFree=true;
    bool lockExists =false;
    for (int i=0; i< holdLocks.size();i++) { // iterate through vector of pairs
        if (holdLocks[i].second == lock) {  // if we find the given pair for the cv
            lockExists=true; //lock held by thread
            
            if (holdLocks[i].first !=currentThread) { // lock held by thread other than current thread
                
                lockFree=false;
                break;
            }
        }
        
    }
    if (lockFree) {
        if (lockExists) { // current thread already holds lock
            interrupt_enable();
            return -1;
        }
        else { // need to give lock to current thread
            std::pair <ucontext_t*,unsigned int > tempPair;
            tempPair.first=currentThread;
            tempPair.second=lock;
            holdLocks.push_back(tempPair);
            
            interrupt_enable();
            
            return 0;
        }
    }
    
    else { // lock thread, run thread from top of readyQ
        if (readyQ.empty()) { // test for deadlock
            interrupt_enable();
            exit(0);
            return 0;
            
        }
        
        // if not deadlock we need to add current thread to blocked thread q and swith to q at top of ready q
        
        bool alreadyBlock=false;
        for (int i=0; i< blockedThreads.size();i++) { // iterate through vector of pairs
            if (blockedThreads[i].second == lock) {  // if we find the given pair for the blocked thread
                alreadyBlock=true;  	//already q for current lock
                blockedThreads[i].first.push(currentThread);
                
            }
        }
        if (!alreadyBlock) { // lock not in block q yet
            std::queue<ucontext_t*> tempQ;
            tempQ.push(currentThread); // start new block q with current thread in it
            // 	std::pair<std::queue <ucontext_t*>,unsigned int> tempPair=<tempQ,lock> // put locked q with representing lock in vector
            blockedThreads.push_back(make_pair(tempQ,lock)); // see above comment
            
            
        }
        // change to top of Ready Q
        ucontext_t* nxtThread = readyQ.front(); // get thread at front of readyq
        readyQ.pop();
        ucontext_t* hold = currentThread; // temporary save of the current thread to push it to the ready queue
        currentThread=nxtThread;
        swapcontext(hold, currentThread); // actually change the running thread
        interrupt_enable();
        return 0;
    }
}











