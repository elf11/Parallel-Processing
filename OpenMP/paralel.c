#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

int Pmin, Pmax, n, T;
static int nrA[1000], nrB[1000], maxPriceA[1000], maxPriceB[1000];

/*
 * Variabile in care retinem : tipul de resurse(A/B)
 * Cost = costul resursei produse in celula curenta
 * CostC = costul resursei complementare celulei curente
 * B = bugetul aferent celului curente
 */

struct Colonists {
  int **Res, **P, **Cost, **B, **CostC;
};

/*
 * year = structura in care retinem matricele pentru mai multi ani,
 * structura de tip Colonists
 */
struct Colonists *year;

FILE *in, *out;

/*
 * Calcularea distantei Manhattan dintre doua celule
 */
int Manhattan(int i, int j, int i1, int j1)
{
  return (abs(i - i1) + abs(j - j1));
}

/*
 * Calcularea costului minim al resursei atat complementare cat si al
 * resursei produse in celula curenta.
 * int y - anul pentru care se realizeaza acest calcul
 * int type - 0 sau 1 in functie de tipul de resursa pentru care se
 *            realizeaza calculul, 0 = resursa curenta, 1 = resursa complentara
 */
void CalculateCostRes(int y, int type)
{
  int i, j, i1, j1, min, dist;

  #pragma omp parallel for private(i, j, i1, j1, min) shared(year) schedule(runtime)

  for (i = 0; i < n; i += 1)
    for (j = 0; j < n; j += 1)
      {
	min = 32000;
	
	for (i1 = 0; i1 < n; i1 += 1)
	  for (j1 = 0; j1 < n; j1 += 1)
	    {
		  dist = Manhattan(i, j, i1, j1);
	      if (year[y].P[i1][j1] + dist < min)
		{
		  if (type == 1)
		    {
		      if (year[y].Res[i1][j1] == 1 - year[y].Res[i][j])
			{	
			  min = year[y].P[i1][j1] + dist;
			}
		    } 
		  else 
		    if (type == 0)
		      {
				if (year[y].Res[i1][j1] == year[y].Res[i][j])
			  {
			    min = year[y].P[i1][j1] + dist;
			  }
		      }
		}
	    }


	if (type == 1) 
	  {
	    year[y].CostC[i][j] = min;
	  }
        else if (type == 0)
	  {
	    year[y].Cost[i][j] = min;
	  }


      }
}

/*
 * Calcularea costului resursei curente si a resursei complementarea,
 * calcularea bugetelor si a preturilor in fiecare an.
 */
void calculateAll()
{
  int i, j, t;

  for (t = 0; t < T; t += 1)
    {
      CalculateCostRes(t, 0);
      CalculateCostRes(t, 1);

#pragma omp parallel for private(i, j) shared(year) schedule(runtime)   
   
      for (i = 0; i < n; i += 1)
	{
	  for (j = 0; j < n; j += 1)
	    {
	      if (year[t].CostC[i][j] > year[t].B[i][j])
		{
		  year[t+1].B[i][j] = year[t].CostC[i][j];
		  year[t+1].P[i][j] = year[t].P[i][j] + (year[t].CostC[i][j] - year[t].B[i][j]);
		  year[t+1].Res[i][j] = year[t].Res[i][j];
		}
	      else
		{
		  if (year[t].CostC[i][j] < year[t].B[i][j])
		    {
		      year[t+1].B[i][j] = year[t].CostC[i][j];
		      year[t+1].P[i][j] = year[t].P[i][j] + (year[t].CostC[i][j] - year[t].B[i][j]) / 2;
		      year[t+1].Res[i][j] = year[t].Res[i][j];
		    }
		  else
		    if (year[t].CostC[i][j] == year[t].B[i][j])
		      {
			year[t+1].B[i][j] = year[t].CostC[i][j];
			year[t+1].P[i][j] = year[t].Cost[i][j] + 1;
			year[t+1].Res[i][j] = year[t].Res[i][j];
		      }
		}

	      if (year[t+1].P[i][j] < Pmin)
		{
		  year[t+1].P[i][j] = Pmin;
		  year[t+1].Res[i][j] = year[t].Res[i][j];
		}

	      if (year[t+1].P[i][j] > Pmax)
		{
		  year[t+1].Res[i][j] = 1 - year[t].Res[i][j];
		  year[t+1].B[i][j] = Pmax;
		  year[t+1].P[i][j] = (Pmin + Pmax) / 2;
		}
		#pragma omp critical
		   if (year[t].Res[i][j] == 0)
	    {
	      nrA[t] += 1;
	      if (year[t].P[i][j] > maxPriceA[t])
		{
		  maxPriceA[t] = year[t].P[i][j];
		}
	    }
	  	  else
	    {
	      nrB[t] += 1;
	      if (year[t].P[i][j] > maxPriceB[t])
		{
		  maxPriceB[t] = year[t].P[i][j];
		}
	    }

	    }
	}

    }
}

int main(int argc, char** argv)
{

  if (argc < 4)
    {
      printf("Programul se ruleaza astfel : %s nr_iteratii fis_in fis_out\n", argv[0]);
      return -1;
    }

  T = atoi(argv[1]);

  in = fopen(argv[2], "r");
  out = fopen(argv[3], "w");
  fscanf(in, "%d", &Pmin);
  fscanf(in, "%d", &Pmax);
  fscanf(in, "%d", &n);

  int i, j;
  year = malloc((T+1) * sizeof(struct Colonists));
  if (!year)
    {
      printf("Alocarea nu a reusit!\n");
      return -1;
    }

  for (i = 0; i <= T; i += 1)
    {
      year[i].Res = (int **)malloc(n * sizeof(int *));
	  if (!year[i].Res)
    {
      printf("Alocarea nu a reusit!\n");
      return -1;
    }
      year[i].P = (int **)malloc(n * sizeof(int *));
	  if (!year[i].P)
    {
      printf("Alocarea nu a reusit!\n");
      return -1;
    }
      year[i].Cost = (int **)malloc(n * sizeof(int *));
	  if (!year[i].Cost)
    {
      printf("Alocarea nu a reusit!\n");
      return -1;
    }
      year[i].B = (int **)malloc(n * sizeof(int *));
	  if (!year[i].B)
    {
      printf("Alocarea nu a reusit!\n");
      return -1;
    }
      year[i].CostC = (int **)malloc(n * sizeof(int *));
	  if (!year[i].CostC)
    {
      printf("Alocarea nu a reusit!\n");
      return -1;
    }

      for (j = 0; j < n; j += 1) 
	{
	  year[i].Res[j] = (int *)malloc(n * sizeof(int));
	  year[i].P[j] = (int *)malloc(n * sizeof(int));
	  year[i].Cost[j] = (int *)malloc(n * sizeof(int));
	  year[i].B[j] = (int *)malloc(n * sizeof(int));
	  year[i].CostC[j] = (int *)malloc(n * sizeof(int));
	}
    }

  while(!feof(in))
    {
      for (i = 0; i < n; i += 1)
	for (j = 0; j < n; j += 1)
	    fscanf(in, "%d", &year[0].Res[i][j]);

      for (i = 0; i < n; i += 1)
	for (j = 0; j < n; j += 1)
	    fscanf(in, "%d", &year[0].P[i][j]);

      for (i = 0; i < n; i += 1)
	for (j = 0; j < n; j += 1)
	    fscanf(in, "%d", &year[0].B[i][j]);
    }

  fclose(in);

  for (i = 0; i <= T; i += 1)
	{
	  nrA[i] = 0;
	  nrB[i] = 0;
	  maxPriceA[i] = 0;
	  maxPriceB[i] = 0;	
	}

  calculateAll();

  for (i = 1; i <= T; i += 1)
    {
      fprintf(out, "%d %d %d %d\n", nrA[i], maxPriceA[i], nrB[i], maxPriceB[i]);
    }

  for (i = 0; i < n; i += 1)
    {
      for (j = 0; j < n; j += 1)
	{
	  fprintf(out, "(%d,%d,%d) ", year[T].Res[i][j], year[T].P[i][j], year[T].B[i][j]);
	}
      fprintf(out, "\n");
    }

  fclose(out);

  for (i = 0; i < n; i += 1)
	{
	  free(year[i].Res);
	  free(year[i].P);
	  free(year[i].Cost);
      free(year[i].B);
	  free(year[i].CostC);
	}

  free(year);

  return 0;
}
