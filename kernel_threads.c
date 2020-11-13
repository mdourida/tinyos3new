
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "util.h"
#include "kernel_streams.h"
#include "kernel_cc.h"


void start_thread(){
  int exitval;

  Task call= CURTHREAD->ptcb->task;

  int argl = CURTHREAD->ptcb->argl;
  void* args = CURTHREAD->ptcb->args;

  exitval = call(argl,args);
  ThreadExit(exitval);

}

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{  
  if(task!=NULL){
    TCB* tcb=spawn_thread(CURPROC,start_thread);

    CURPROC->thread_count++;                        /*auxanei ta thread tou pcb*/
    PTCB* ptcb=(PTCB*)xmalloc(sizeof(PTCB));
    ptcb->refcount=1;
    ptcb->task=task;
    ptcb->argl=argl;

    if(args!=NULL) {
     ptcb->args=args;
    }
    else{
     ptcb->args=NULL;
    }

    ptcb->exited=0;
    ptcb->detached=0;
    ptcb->exit_cv=COND_INIT;
    ptcb->exitval=CURPROC->exitval;
    rlnode_init(&ptcb->ptcb_list_node,ptcb);

    tcb->ptcb=ptcb;
    ptcb->tcb=tcb;                        /*sundeei to tcb me ptcb*/
    rlist_push_back(& CURPROC->ptcb_list, & ptcb->ptcb_list_node);
    wakeup(ptcb->tcb);
    return (Tid_t) ptcb;
    }
  return NOTHREAD;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) (CURTHREAD->ptcb);
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
	PTCB* ptcb= (PTCB*) tid;
  PTCB* ptcb_found=NULL;
  if(rlist_find(&CURPROC->ptcb_list, ptcb,NULL)){
   ptcb_found=ptcb;
  }
  else{
    return -1;
  }
  if((Tid_t) CURTHREAD==tid){   //can not join self
    return -1;
  }
  if(ptcb_found->exited==1 || ptcb->detached==1){
    return -1;
  }
  ptcb_found->refcount++;
  while(ptcb_found->exited!=1 && ptcb->detached!=1){ //when a thread detached or exited wake up
    kernel_wait(&ptcb_found->exit_cv,SCHED_USER);
  }
  ptcb_found->refcount--;
  if(ptcb_found->refcount==0){
    rlist_remove(& ptcb->ptcb_list_node);
    free(ptcb);
  }
  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	PTCB* ptcb= (PTCB*) tid;
  PTCB* ptcb_found=NULL;

  if(rlist_find(&CURPROC->ptcb_list,ptcb,NULL)){
   ptcb_found=ptcb;
   //fprintf(stdout, "%d\n", ptcb->exited);
  }else{
    //fprintf(stdout, "%s\n", "bbb");
    return -1;
  }

  if(ptcb_found->exited==1 ){
   return -1;
  }
   //fprintf(stdout, "%d\n", ptcb->exited);
  ptcb_found->detached=1;
  kernel_broadcast(&ptcb_found->exit_cv);
  //fprintf(stdout, "%d\n", ptcb->exited);
  ptcb_found->refcount=1;     /*non joinable thread*/
  return 0;
}
/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{


  PTCB* ptcb=CURTHREAD->ptcb;
  ptcb->refcount--;
  ptcb->exited=1;
  ptcb->exitval=exitval;

  CURPROC->thread_count--;
  // kernelbroadcast
  kernel_broadcast(& ptcb->exit_cv);

  /*an einai to teleytaio thread*/
 if (CURPROC->thread_count==1){
   PCB *curproc = CURPROC;  /* cache for efficiency */

  /* Do all the other cleanup we want here, close files etc. */
  if(curproc->args) {
    free(curproc->args);
    curproc->args = NULL;
  }

  /* Clean up FIDT */
  for(int i=0;i<MAX_FILEID;i++) {
    if(curproc->FIDT[i] != NULL) {
      FCB_decref(curproc->FIDT[i]);
      curproc->FIDT[i] = NULL;
    }
  }

  /* Reparent any children of the exiting process to the 
     initial task */
  PCB* initpcb = get_pcb(1);
  while(!is_rlist_empty(& curproc->children_list)) {
    rlnode* child = rlist_pop_front(& curproc->children_list);
    child->pcb->parent = initpcb;
    rlist_push_front(& initpcb->children_list, child);
  }

  /* Add exited children to the initial task's exited list 
     and signal the initial task */
  if(!is_rlist_empty(& curproc->exited_list)) {
    rlist_append(& initpcb->exited_list, &curproc->exited_list);
    kernel_broadcast(& initpcb->child_exit);
  }

  /* Put me into my parent's exited list */
  if(curproc->parent != NULL) {   /* Maybe this is init */
    rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
    kernel_broadcast(& curproc->parent->child_exit);
  }

  /* Disconnect my main_thread */
  curproc->main_thread = NULL;

  /* Now, mark the process as exited. */
  curproc->pstate = ZOMBIE;
  curproc->exitval = exitval;
 }
  if(ptcb->refcount==0){
    rlist_remove(& ptcb->ptcb_list_node);
    free(ptcb);
  }
 
 kernel_sleep(EXITED, SCHED_USER);
}
