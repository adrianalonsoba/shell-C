#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>

int pipes[1024][2];

typedef struct Command Command;

struct Command{
	char *arguments[1024];
};


int 
tokenize(char *str, char **args,char token){
	int j=0;
	int i=0;
	
	for(;;){
		if((str[j]==token)||(str[j])=='\n'||
			(str[j]=='\r')||(str[j]=='\t')){
			j++;
		}else{
			break;
		}	     
	}
	
	args[i]=&str[j];
	i++;

	while(str[j]!='\0'){
		if((str[j]==token)||(str[j])=='\n'){
			str[j]='\0';
			j++;
			for(;;){
				if((str[j]==token)||(str[j])=='\n'||
				   (str[j]=='\r')||(str[j]=='\t')){
					   j++;
			    }else{
					break;
				}	     
			}
			if(str[j]!='\0'){
				args[i]=&str[j];
				i++;
			}	
		}else{
			j++;
		}
	}
	return i;
}

char* 
concat(char *s1, char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+2);
    strcpy(result, s1);
    strcat(result,"/");
    strcat(result, s2);
    return result;
}

void
createpipes(int pos, int numpipes){
	
	if(pos==0){
		dup2(pipes[0][1],1);
	}else if(pos==numpipes){
		dup2(pipes[pos-1][0],0);
	}else{
		dup2(pipes[pos-1][0],0);
		dup2(pipes[pos][1],1);
	}
}

void
closepipes(int numcmds){
	int i;
	int numpipes=numcmds-1;
	for(i=0;i<numpipes;i++){
		close(pipes[i][0]);
		close(pipes[i][1]);
	}
}

void
makeinredir(char *fin){
	int fd;
	fd=open(fin,O_RDONLY);
	if (fd<0)
		err(1,"%s\n",fin);
	dup2(fd,0);
	close(fd);
}

void
makeoutredir(char *fout){
	int fd;
	fd=open(fout,O_WRONLY|O_TRUNC|O_CREAT,0644);
	if (fd<0)
		err(1,"%s\n",fout);
	dup2(fd,1);
	close(fd);
}

void
executecmd(Command cmd,int numcmds,int pos,
		  int outredir, char *fout,int inredir, char *fin,int bg){
			  
	char* cmdpath0;		  
	char* cmdpath1;
	char* cmdpath2;
	char* cmdpath3;
	int numpipes = numcmds-1;
	int fd;
	
	if(numcmds>1){
		createpipes(pos,numpipes);
		closepipes(numcmds);
	}
	
	if(!inredir && (pos==0) && bg){
		  fd = open("/dev/null",O_RDONLY);
		  dup2(fd,0);
		  close(fd);
	 }
	 
	if(inredir && (pos==0))
		makeinredir(fin);	
	
	if(outredir && (pos==numpipes))
		makeoutredir(fout);
	
	cmdpath0=concat(".",cmd.arguments[0]);
	if(access(cmdpath0,X_OK)==0){
		execv(cmdpath0,&cmd.arguments[0]);
		exit(1);
	}
	free(cmdpath0);

	cmdpath1=concat("/bin",cmd.arguments[0]);
	if(access(cmdpath1,X_OK)==0){
		execv(cmdpath1,&cmd.arguments[0]);
		exit(1);
	}
	free(cmdpath1);	
	
	cmdpath2=concat("/usr/bin",cmd.arguments[0]);
	if(access(cmdpath2,X_OK)==0){
		execv(cmdpath2,&cmd.arguments[0]);
		exit(1);
	}
	free(cmdpath2);		
	
	cmdpath3=concat("/usr/local/bin",cmd.arguments[0]);
	if(access(cmdpath3,X_OK)==0){
		execv(cmdpath3,&cmd.arguments[0]);
		exit(1);
	}
	free(cmdpath3);	
	
	printf("%s: invalid command\n", cmd.arguments[0]);	
	
	exit(1);
}

int 
changedir(Command cmds[64]){
	char *envhome;
	int cdin=0;
	
	if(strcmp(cmds[0].arguments[0],"cd")==0){
		if(cmds[0].arguments[1]==NULL){
			envhome=getenv("HOME");
			chdir(envhome);
			cdin=1;
		}else{
			if(chdir(cmds[0].arguments[1])<0){
				fprintf(stderr,"%s: invalid dir\n",cmds[0].arguments[1]);
			}
			cdin=1;
		}
	}
	return cdin;
}

int 
isson(int pid[64],int pi,int numcmds){
	int i;
	for(i=0;i<numcmds;i++){
		if (pid[i]==pi)
			return 1;
	}
	return 0;
}

void
createsons(Command cmds[64], int numcmds,int outredir,
		   char *fout,int inredir, char *fin, int bg){
	int pid[64];
	int i;
	int pi;
	int w;
 
	if(!changedir(cmds)){
		for(i=0;i<numcmds-1;i++){
			pipe(pipes[i]);
		}
		for(i=0;i<numcmds;i++){
			pid[i]=fork();
			if(pid[i]<0){
				fprintf(stderr,"fork failed\n");
				exit(1);				
			}else if(pid[i]==0){
				executecmd(cmds[i],numcmds,i,outredir,fout,inredir,fin,bg);
			}
		}
		closepipes(numcmds);

		if(!bg){
			w=0;
			while(w!=numcmds){
				pi=wait(NULL);
				if (pid<0)
					break;
				if (isson(pid,pi,numcmds))
					w++;
			}
		}
	}
}

int
tokenv(char *line){
	int nenv;
	char *aux1[1024];
	char *aux2[1024];
	char *envs;
	char *envd;
	
	nenv=tokenize(line,aux1,'=');
	if(nenv==2){
		tokenize(aux1[1],aux2,' ');
		envd=aux2[0];
		tokenize(aux1[0],aux2,' ');
		envs=aux2[0];
		setenv(envs,envd,1);
	}
	return nenv;	
}

void
saveenvs(char *args[], int nargs){
	int i;
	char *env;
	
	for(i=0;i<nargs;i++){
		if(args[i][0]=='$'){
			env = &args[i][1];
			args[i]=getenv(env);
		} 
	}
	
}

int
background(char *arg){
	int i;
	int back=0;
	for(i=0;i<strlen(arg);i++){
		if (arg[i]=='&'){
			arg[i]='\0';
			back=1;
		}
	}
	return back;
	
}

void
procline(char line[1024], Command cmds[64]){
	int ncmds;
	int i;
	int nargs;
	int nrin=0;
	int nrout=0;
	int inredir=0;
	int outredir=0;
	int bg=0;
	char *fin;
	char *fout;
	char *cmd[1024];
	char *aux1[1024];
	char *aux2[1024];
	
	if(background(line)){
		bg=1;
	}
	
	if(tokenv(line)!=2){
		nrout=tokenize(line,aux1,'>');
		if(nrout==2){
			outredir=1;
			tokenize(aux1[1],aux2,' ');
			fout=aux2[0];
			if (fout[0]=='$')
				fout=getenv(&fout[1]);

		}

		nrin=tokenize(line,aux1,'<');
		if(nrin==2){
			inredir=1;
			tokenize(aux1[1],aux2,' ');
			fin=aux2[0];
			if (fin[0]=='$')
				fin=getenv(&fin[1]);
		}
		
		strcpy(line,aux1[0]);
		
		ncmds=tokenize(line,cmd,'|');
		for(i=0;i<ncmds;i++){
			nargs=tokenize(cmd[i],cmds[i].arguments,' ');
			cmds[i].arguments[nargs]=NULL;
			saveenvs(cmds[i].arguments,nargs);
		}
		
		createsons(cmds,ncmds,outredir,fout,inredir,fin,bg);
	}
}

int
main(int argc, char *argv[]){
	char *s;
	int leave=0;
	char line[1024];
	Command cmds[64];
	
	while(!leave){
		memset(cmds,0,sizeof(Command)*64);
		printf("shell$ ");
		s=fgets(line,1024,stdin);
		if(s!=NULL){
			procline(line,cmds);
		}else{
			leave=1;
		}	
	}
	exit(0);
}
