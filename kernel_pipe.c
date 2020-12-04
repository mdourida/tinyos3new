
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_cc.h"

static file_ops reader ={
	.Open=NULL,
	.Read=pipe_read,
	.Write=useless,
	.Close=pipe_reader_close
};
static file_ops writer ={
	.Open=NULL,
	.Read=useless,
	.Write=pipe_write,
	.Close=pipe_writer_close
};

int useless(void* pipecb_t, const char *buf, unsigned int n){
	return -1;
}


int sys_Pipe(pipe_t* pipe)
{ 
	Fid_t fid[2];//new file id for read and write
  	FCB* fcb[2]; //new file control block for read and write
  	//FCB_reserve(2,fid,fcb); 
	if (FCB_reserve(2, fid, fcb)==0)  //gia test 2 na mhn skaei
	{
		return -1;
	}

  	pipe_cb* p=xmalloc(sizeof(pipe_cb));//reserve space for 

  	pipe->read = fid[0];
	pipe->write = fid[1];

	p->reader=0;
	p->writer=0;
	p->has_space=COND_INIT;
	p->has_data=COND_INIT;
	p->w_position=0;
	p->r_position=0;

	fcb[0]->streamobj=p;
	fcb[1]->streamobj=p;
	fcb[0]->streamfunc=&reader;
	fcb[1]->streamfunc=&writer;
    
    p->reader=fcb[0];
    p->writer=fcb[1];

return 0;

}

int pipe_write(void* this,const char *buf, unsigned int size){
	pipe_cb* p=(pipe_cb*)this;
	assert(p!=NULL);

	while(isFull(p)==0){
		kernel_wait(&p->has_space,SCHED_PIPE);
	}

	if(p->reader==NULL || p->writer==NULL){
		return -1;
	}

	int count=0;
	while(count<size && count<PIPE_BUFFER_SIZE){
		p->BUFFER[p->w_position]=buf[count]; //copy in pipe buffer  
        p->w_position=(p->w_position+1)%PIPE_BUFFER_SIZE; //next write position for bounded buffer
        count++; 
        if ((p->w_position+1)%PIPE_BUFFER_SIZE==p->r_position){
        	kernel_broadcast(&(p->has_data)); 
    		kernel_wait(&(p->has_space), SCHED_PIPE);
    		break;

        }
	}

	if (count==size || count==PIPE_BUFFER_SIZE){
		kernel_broadcast(&(p->has_data));
  		return count; //returns number of bytes copied in pipe buffer

}
}


int pipe_read(void* this, char *buf, unsigned int size){

 pipe_cb* p=(pipe_cb*) this;

 assert(p!=NULL);
 if(isEmpty(p)==0 && p->writer==NULL){ //den 8a gemisei pote
 	return 0;                          //elegxos gia na mhn skaei to test 4
 }

 if(p->reader==NULL){
 	return -1;
 }

 while(isEmpty(p)==0){
   kernel_wait(&p->has_data,SCHED_PIPE);
 }

 int count=0;
//periptwsi pou o writer exei kleisei kai o reader prepei na diavasei o,ti exei meinei
if (p->writer==NULL){
	while(p->r_position!=p->w_position && count<size && count<PIPE_BUFFER_SIZE){
		buf[count]=p->BUFFER[p->r_position];
   		p->BUFFER[p->r_position]=0;        //read and delete element
   		p->r_position=(p->r_position+1)%PIPE_BUFFER_SIZE; 
   		count++;

	}return count;

}

 while(count<size && count<PIPE_BUFFER_SIZE ){
   buf[count]=p->BUFFER[p->r_position];
   p->BUFFER[p->r_position]=0;        //read and delete element
   p->r_position=(p->r_position+1)%PIPE_BUFFER_SIZE; 
   count++;
   if ((p->w_position+1)%PIPE_BUFFER_SIZE==p->r_position){
        	kernel_broadcast(&(p->has_space)); 
    		kernel_wait(&(p->has_data), SCHED_PIPE);
    		break;

        }
  }if (count==size || count==PIPE_BUFFER_SIZE){
		kernel_broadcast(&(p->has_space));
  		return count; //returns number of bytes copied in pipe buffer

	}
}

int pipe_writer_close(void* this){
  pipe_cb* p=(pipe_cb*) this;
  assert(p!=NULL);
  p->writer=NULL;  //close write end
   kernel_broadcast(&(p->has_data));

  if(p->reader==NULL){ //if read end closed free pipe
  	p=NULL;
  	free(p);
  }
  return 0;
}

int pipe_reader_close(void* this){
  pipe_cb* p=(pipe_cb*) this;
  assert(p!=NULL);
  p->reader=NULL;
  kernel_broadcast(&(p->has_space));

    if(p->writer==NULL){
  	free(p);
  	p=NULL;
  }
   return 0;
}

int isFull(void *this){
	pipe_cb* p=(pipe_cb*)this;
	for(int i=0; i<PIPE_BUFFER_SIZE; i++){
		if(p->BUFFER[i]==NULL){
          return -1;
		}
	}
 return 0;
}

int isEmpty(void* this){
	pipe_cb* p=(pipe_cb*)this;
	for(int i=0; i<PIPE_BUFFER_SIZE; i++){
		if(p->BUFFER[i]!=NULL){
          return -1;
		}

	}
 return 0;
}