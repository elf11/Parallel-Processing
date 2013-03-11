#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <time.h>
#include "mpi.h"

using namespace std;
using namespace MPI;

#define SONDAJ	0
#define ECOU	1
#define BUFFER_SIZE 100
#define TAG		20
#define BROADCAST 1000
#define CLOSE 59

struct message
{
	int src, dest;
	int leader;
	char s[100];
};

void readTop(const char *filename, int rank, int **top);
static void updateTop(int **result, int **aux, int *rTable, int n, int rank, int child);
int *discoverTop(int **top, int n, int rank);

void readTop(const char *filename, int rank, int **top)
{
    char buffer[BUFFER_SIZE];
    char *p;
    int k, numTasks;

    FILE *f = fopen(filename, "rt");

    while (fgets(buffer, BUFFER_SIZE, f))
    {
        p = strtok(buffer, ":");
        if (atoi(p) == rank)
        {
            while ((p = strtok(NULL, " ")) != NULL)
            {
                k = atoi(p);
                if (k != rank)
                    top[rank][k] = top[k][rank] = 1;
            }
            break;
        }
    }

    fclose(f);
}

/**
  * Algoritm de descoperire a topologiei.
  * Toate procesele mai putin root(initiatorul) asteapta mesaje de tip
  * sondaj de la parinte. Fiecare proces trimite mesaje de tip sonda tuturor vecinilor, mai putin parintelui.
  * Nodul curent asteapta de la toti vecinii mai putin parintele
  * un mesaj (sonda sau ecou), daca mesajul e sonda, trimit inapoi topologie nula vecinului, si vecinul poate fi scos din topologie, in sensul ca daca ar ramane acolo am realiza bucle.
  * Daca mesajul e un ecou, updatez topologia.
  * Trimit topologia parintelui.
  * In tabela de rutare o intrare redundanta va fi reprezentata prin -1.
  */
int *discoverTop(int **top, int nrNodes, int rank)
{
	MPI_Request send_request, recv_request;
    	MPI_Status status;
	vector<int> badNodes(100);
	int nrBad = 0;
	int **tmpTop;
	int *rTable;
	int par, node, childNr, type;
	int cnt = 0;

	par = -1; /* initial nu avem un parinte, pentru nodul 0 */
	
	tmpTop = (int **)malloc((nrNodes - 1) * sizeof(int*));
	for (int i = 0; i < nrNodes; i += 1)
	{
		tmpTop[i] = (int *)calloc(nrNodes, sizeof(int));
	}

	rTable = (int *)malloc(nrNodes * sizeof(int));

	if (rank != 0)
	{
		MPI_Recv(&type, 1, MPI_INT, MPI_ANY_SOURCE, TAG, MPI_COMM_WORLD, &status);
		//MPI_Wait(&recv_request, &status);
		par = status.MPI_SOURCE;
	
		for (int i = 0; i < nrNodes; i += 1)
		{
			if (i != par && top[rank][i] == 1)
			{
				//printf("Sunt %i si Trimit SONDAJ la %i\n", rank, i);
				type = SONDAJ;
				MPI_Send(&type, 1, MPI_INT, i, TAG, MPI_COMM_WORLD);
				cnt += 1;
			}
		}
	} else
	{
		for (int i = 0; i < nrNodes; i += 1)
		{
			if (top[rank][i] == 1)
			{
				//printf("Sunt sef si trimit SONDAJ la %i\n", i);
				type = SONDAJ;
				MPI_Send(&type, 1, MPI_INT, i, TAG, MPI_COMM_WORLD);
				cnt += 1;
			}
		}
	}



	for (int i = 0; i < nrNodes - 1; i += 1)
	{
		if (i != rank)
		{
			rTable[i] = par;
		} else
		{
			rTable[i] = rank;
		}
	}

	childNr = 0;
	
	for (int i = 0; i < nrNodes; i += 1)
	{
		if (i != par && top[rank][i] == 1)
		{
			childNr += 1;
		}
	}


	/**
	  * Asteptam mesaje de la toti copii.
	  */

	childNr = cnt;
	while (childNr > 0)
	{
		MPI_Recv(&type, 1, MPI_INT, MPI_ANY_SOURCE, TAG, MPI_COMM_WORLD, &status);
		//MPI_Wait(&recv_request, &status);
		int p = status.MPI_SOURCE;

		switch(type)
		{
		case ECOU:
			for (int j = 0; j < nrNodes; j += 1)
			{
				MPI_Recv(tmpTop[j], nrNodes, MPI_INT, p, TAG, MPI_COMM_WORLD, &status);
				//MPI_Wait(&recv_request, &status);
			}
			updateTop(top, tmpTop, rTable, nrNodes, rank, p); 
			childNr -= 1;
			break;
		case SONDAJ:
			type = ECOU;
			MPI_Send(&type, 1, MPI_INT, p, TAG, MPI_COMM_WORLD);
			memset(tmpTop[0], 0, nrNodes * sizeof(int));
			for (int j = 0; j < nrNodes; j += 1)
			{
				MPI_Send(tmpTop[0], nrNodes, MPI_INT, p, TAG, MPI_COMM_WORLD);
			}
			badNodes.at(nrBad) = p;
			nrBad += 1;
			break;
		}
	}

	for (int i = 0; i < badNodes.size(); i += 1)
	{
		node = badNodes[i];
		for (int j = 0; j < nrNodes; j += 1)
		{
			top[node][j] = top[j][node] = 0;
		}
	}

	if (rank != 0)
	{
		if (par >= 0)
		{	
			type = ECOU;
			MPI_Send(&type, 1, MPI_INT, par, TAG, MPI_COMM_WORLD);
			for (int j = 0; j < nrNodes; j += 1)
			{
				MPI_Send(top[j], nrNodes, MPI_INT, par, TAG, MPI_COMM_WORLD);
			}
		}
	}

	for (int i = 0; i < nrNodes; i += 1)
	{
		free(tmpTop[i]);
	}
	free(tmpTop);

	badNodes.clear();

	return rTable;
}

/**
  * Update pentru topologie reprezentata printr-o matrice de adiacenta.
  */

static void updateTop(int **result, int **aux, int *rTable, int n, int rank, int child)
{
	int none;

	for (int i = 0; i < n; i += 1)
	{
		none = 1;
		for (int j = 0; j < n; j += 1)
		{
			if (aux[i][j])
			{
				result[i][j] = 0;
				none = 0;
			}
		}
		if (i != rank && !none)
		{
			rTable[i] = child;
		}
	}
}

int main(int argc, char **argv)
{
	MPI_Request send_request, recv_request;
   	MPI_Status status;
	int **top;
	int *rTable;
	int numTasks, rank, recvLen, sendLen, nrMes;
	struct message recv, send;
	char buf[200];
	char *p;


	if (argc < 3)
	{
		printf("Tema se ruleaza astfel : %s fis_top fis_mes\n", argv[0]);
		return -1;
	}

	FILE *topologie, *mess;
	topologie = fopen(argv[1], "rt");
	if (!topologie)
	{
		printf("Fisierul furnizat pentru topologie %s nu este valid\n", argv[1]);
		return -1;
	}
	fclose(topologie);

	mess = fopen(argv[2], "rt");
	if (!mess)
	{
		printf("Fisierul furnizat pentru mesaje %s nu este valid\n", argv[2]);
		return -1;
	}

	MPI_Init(&argc, &argv);
    	MPI_Comm_size(MPI_COMM_WORLD, &numTasks);
   	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	top = (int **)malloc(numTasks * sizeof(int *));
	for (int i = 0; i < numTasks; i += 1)
	{
		top[i] = (int *)calloc(numTasks, sizeof(int *));
	}
	
	/**
	  * Citesc topologia din fisier.
	  */
	readTop(argv[1], rank, top);

	printf("Afisez topologia pentru procesul %i\n", rank);
	for (int i = 0; i < numTasks; i += 1)
	{
		printf("%i ", top[rank][i]);
	}
	printf("\n");

	MPI_Barrier(MPI_COMM_WORLD);

	/**
	  * Creez tabela de rutare pentru procesul curent
	  */
	rTable = discoverTop(top, numTasks, rank);
	//sincronizare procese
	MPI_Barrier(MPI_COMM_WORLD);

	for (int i = 0; i < numTasks; i += 1)
	{
		printf("Tabela lui %i contine pe %i\n", rank, rTable[i]);
	}

	
	for (int i = 0; i < numTasks; i += 1)
	{
		printf("Tabela lui %d: catre %d o iei via %d\n", rank, i, rTable[i]);
		printf("\n");
		fflush(stdout);
	}

	for (int i = 0; i < numTasks; i += 1)
	{
		free(top[i]);
	}
	
	//citire si rutare mesaje conform topologiei
	fgets(buf, 200, mess);
	nrMes = atoi(buf);

	for (int i = 0; i < nrMes; i += 1)
	{
		fgets(buf, 200, mess);
		p = strtok(buf, " ");
		send.src = atoi(p);
		if (send.src != rank) continue;

		p = strtok(NULL, " ");
		if (p[0] != 'B')
		{
			send.dest = atoi(p);
		} else
		{
			send.dest = BROADCAST;
		}

		p = strtok(NULL, "\n");
		strcpy(send.s, p);

		if (send.dest != BROADCAST)
		{
			printf("Rank %i :  am trimis mesaj spre %i\n", rank, send.dest);
			MPI_Send(&send, sizeof(message), MPI_CHAR, rTable[send.dest], TAG, MPI_COMM_WORLD);
		} else
		{
			printf("Rank %i : am trimis un mesaj de BROADCAST\n", rank);
			for (int j = 0; j < numTasks; j += 1)
			{
				if (top[rank][j] == 1 && top[rank][j] != -1)
				{
					MPI_Send(&send, sizeof(message), MPI_CHAR, rTable[rank], TAG, MPI_COMM_WORLD);
				}
			}
		}
	}

	/**
	 * Trimit mesaj de inchidere.
	 */
	send.src = rank;
	send.dest = -1;
	strcpy(send.s, "close");

	for (int i = 0; i < numTasks; i += 1)
	{
		if (top[rank][i] == 1)
		{
			MPI_Send(&send, sizeof(message), MPI_CHAR, rTable[i], TAG, MPI_COMM_WORLD);
		}
	}
	/**
	 * Primire mesaje.
	 */
	int cnt = numTasks;
	int source;
	while (cnt > 0)
	{
		MPI_Recv(&recv, sizeof(message), MPI_CHAR, MPI_ANY_SOURCE, TAG, MPI_COMM_WORLD, &status);
		source = status.MPI_SOURCE;
		if (recv.dest == -1)
		{
			cnt -= 1;
		} else 
		{
			if (recv.dest == rank)
			{
				printf("Rank %i : am primit mesaj de la %i : %s\n", rank, recv.src, recv.s);
			} else
			{
				if (recv.dest != BROADCAST)
				{
					printf("Rank %i : am primit mesaj de la %i pentru %i. Il trimit la %i\n", rank, recv.src, recv.dest, rTable[recv.dest]);
					MPI_Send(&recv, sizeof(message), MPI_CHAR, rTable[recv.dest], TAG, MPI_COMM_WORLD);
				} else
				{
					printf("Rank %i : am primit un mesaj de broadcast de la %i : %s\n", rank, recv.src, recv.s);
					for (int i = 0; i < numTasks; i += 1)
					{
						if (top[rank][i] == 1 && i != source)
						{
							MPI_Send(&recv, sizeof(message), MPI_CHAR, rTable[i], TAG, MPI_COMM_WORLD);
						}
					}	
				}
			}
		}
	}

	MPI_Barrier(MPI_COMM_WORLD);

	/**
	  * Alegerea unui lider si a unui secund.
	  */
	message vote;
	int aux;
	int leader = -1,snd = -1;

	if (rank != 0)
	{
		srand(time(NULL) / rank);
	}
	vote.leader = -1;
	vote.src = -1;
	leader = -1;
	snd = -1;
	if (rank == 0)
	{
		int *leaderV, *secondV;
		int nr, ind, ok;

		leaderV = (int *)calloc(numTasks, sizeof(int));
		secondV = (int *)calloc(numTasks, sizeof(int));

		while (leader == -1 || snd == -1)
		{
			vote.dest = -1;
			vote.leader = leader;
			vote.src = snd;

			for (int i = 0; i < numTasks; i += 1)
			{
				if (top[rank][i] == 1)
				{
					MPI_Send(&vote, sizeof(message), MPI_CHAR, rTable[i], TAG, MPI_COMM_WORLD);
				}
			}

			cnt = numTasks - 1;
			
			if (leader == -1)
			{
				aux = 0;
				while (aux == 0)
				{
					aux = rand() % numTasks;
				}
				leaderV[aux] = leaderV[aux] + 1;
			}

			if (snd == -1)
			{
				aux = 0;
				while (aux == 0)
				{
					aux = rand() % numTasks;
				}
				secondV[aux] = secondV[aux] + 1;
			}

			// Asteptam mesajele de la toate celelalte noduri
			while (cnt > 0)
			{
				MPI_Recv(&vote, sizeof(message), MPI_CHAR, MPI_ANY_SOURCE, TAG, MPI_COMM_WORLD, &status);
				cnt -= 1;
				if (leader == -1)
				{
					leaderV[vote.leader] += 1;
				}
				if (snd == -1)
				{
					secondV[vote.src] += 1;
				}
			}

			// numaram voturile si stabilim leaderii
			nr = -1;
			for (int i = 0; i < numTasks; i += 1)
			{
				if (leaderV[i] > nr)
				{
					nr = leaderV[i];
					ok = 0;
					ind = i;
				} else
				{
					if (leaderV[i] == nr)
					{
						ok = 1; //am gasit egalitate
					}
				}
			}

			if (ok == 0)
			{
				//am gasit leaderul
				leader = ind;
			}
			// stabilim secundul
			nr = -1;
			for (int i = 0; i < numTasks; i += 1)
			{
				if (secondV[i] > nr)
				{
					nr = secondV[i];
					ok = 0;
					ind = i;
				} else
				{
					if (secondV[i] == nr)
					{
						ok = 1;
					}
				}
			}
			if (ok == 0)
			{
				//am gasit leaderul
				snd = ind;
			}
		}

		//trimite rezultatul restului de procese
		vote.dest = -1;
		vote.leader = leader;
		vote.src = snd;

		for (int i = 0; i < numTasks; i += 1)
		{
			if (top[rank][i] == 1)
			{
				MPI_Send(&vote, sizeof(message), MPI_CHAR, rTable[i], TAG, MPI_COMM_WORLD);
			}
		}
	} else
	{
		//fiecare proces primeste invitatia la vot, trimite mai departe
		// copiilor sai si apoi voteaza trimitand rezultatul liderului

		while (leader == -1 || snd == -1)
		{
			MPI_Recv(&vote, sizeof(message), MPI_CHAR, MPI_ANY_SOURCE, TAG, MPI_COMM_WORLD, &status);
			int pp = status.MPI_SOURCE;
			if (vote.dest == -1)
			{
				for (int i = 0; i < numTasks; i += 1)
				{
					if (top[rank][i] == 1 && i != pp)
					{
						MPI_Send(&vote, sizeof(message), MPI_CHAR, rTable[i], TAG, MPI_COMM_WORLD);
						vote.dest = 0;
						
						if (leader == -1)
						{
							if (vote.leader != -1)
							{
								leader = vote.leader;
							}
							aux = rank;
							while (aux == rank)
							{
								aux = rand() % numTasks;
							}
							vote.leader = aux;
						}

						if (snd == -1)
						{
							if (vote.src != -1)
							{
								snd = vote.src;
							}
							aux = rank;
							while (aux == rank)
							{
								aux = rand() % numTasks;
							}
							vote.src = aux;
						}

						if (leader == -1 || snd == -1)
						{	
							MPI_Send(&vote, sizeof(message), MPI_CHAR, rTable[vote.dest], TAG, MPI_COMM_WORLD);
						}

					} else
					{
						MPI_Send(&vote, sizeof(message), MPI_CHAR, rTable[vote.dest], TAG, MPI_COMM_WORLD);
					}
				}
			}
		}
	}

	printf("Rank %i: Leader %i, Second %i\n", rank, leader, snd);

	free(top);

	free(rTable);

	fclose(mess);

	MPI_Finalize();

	return 0;
}


