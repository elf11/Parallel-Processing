#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <math.h>

#define WORKTAG (1)
#define DIETAG (2)
#define NUM_COLOR (256)
#define FALSE_MESSAGE (1000)

// open a file and check if everything went alright
#define  openFile(f, filename, mode) { \
  f = fopen(filename, mode); \
  \
  if (!f) \
  { \
    fprintf(stderr, "PGM error: file could not be opened\n"); \
    return -1; \
  } \
}

/* Local functions */

static void master(char *input, char *output);
static void slave(void);
int *Mandelbrot(double x_min, double x_max, double y_min, double y_max, double resolution, int MAX_STEPS, int width, int height);
int *Julia(double x_min, double x_max, double y_min, double y_max, double resolution, int MAX_STEPS, double complex_a, double complex_b, int width, int height); 

/* Write PGM file */
int 
writePGM(const char *fileName, int width, int height, int **image)
{
  FILE *f;
  int i, j;

  openFile(f, fileName, "wt");

  fprintf(f, "P2\n");
  fprintf(f, "%d %d\n", width, height);
  fprintf(f, "%d\n", 255);

  for (i = height - 1; i >= 0; i -= 1)
    {
      for (j = 0; j < width; j += 1)
	{
	  fprintf(f, "%2d ", image[i][j]);
	 }
    }

   fclose(f);
   
   return 0;    
}

int 
main(int argc, char **argv)
{
  int myrank;

  if (argc < 3)
    {
      printf("Programul se apeleaza astfel: fis_in fis_out\n");
      return -1;
    }

  /* Initialize MPI */
  MPI_Init(&argc, &argv);

  /* Find out my identity in the default communicator */
  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
  if (myrank == 0)
    {
      master(argv[1], argv[2]);
    } else 
    {
      slave();
    }

  /* Shut down MPI */
  MPI_Finalize();

  return 0;
}

static void
master(char *inputName, char *outputName)
{
  int ntasks, rank, type, MAX_STEPS, returnType, writeOut, width, height, newHeight, difference, i, j, k, startLine, ok;
  double x_min, x_max, y_min, y_max, resolution, complex_a, complex_b;
  int **image;
  MPI_Status status;
  //char buffer[200];
  double buffer[100];
  int *smallOut;
  

  /* Find out how many processes there are in the default communicator */
  MPI_Comm_size(MPI_COMM_WORLD, &ntasks);

  /* Open the input/output files and read from the input one */
  FILE *in = fopen(inputName, "r");

  fscanf(in, "%d", &type);
  fscanf(in, "%lf", &x_min);
  fscanf(in, "%lf", &x_max);
  fscanf(in, "%lf", &y_min);
  fscanf(in, "%lf", &y_max);
  fscanf(in, "%lf", &resolution);
  fscanf(in, "%d", &MAX_STEPS);
  if (type == 1)
    {
      fscanf(in, "%lf", &complex_a);
      fscanf(in, "%lf", &complex_b);
    }

  fclose(in);

  width = floor((x_max - x_min) / resolution);
  height = floor((y_max - y_min) / resolution);

  /* Allocate space for the image */
  image = (int **)malloc(height * sizeof(int *));
  for (i = 0; i < height; i += 1)
    {
      image[i] = (int *)malloc(width * sizeof(int));
    }

  newHeight = ceil((double)height / ntasks);

  /* Compute the difference between the actual height of the image and the height the master will get after gathering all the chunks from the workers */
  difference = height - (newHeight * (ntasks - 1));

  /* Create a buffer to be send to the worker threads */
  if (type == 1)
    {
      buffer[0] = type;
      buffer[1] = x_min;
      buffer[2] = x_max;
      buffer[3] = y_min;
      buffer[4] = y_max;
      buffer[5] = resolution;
      buffer[6] = MAX_STEPS;
      buffer[7] = width;
      buffer[8] = newHeight;
      buffer[9] = complex_a;
      buffer[10] = complex_b;
    }
  else
    {
      buffer[0] = type;
      buffer[1] = x_min;
      buffer[2] = x_max;
      buffer[3] = y_min;
      buffer[4] = y_max;
      buffer[5] = resolution;
      buffer[6] = MAX_STEPS;
      buffer[7] = width;
      buffer[8] = newHeight;
      buffer[9] = FALSE_MESSAGE;
      buffer[10] = FALSE_MESSAGE;
    }
    
  /* Send the buffer to all the slaves, keep one chunk of the work for the master thread */
  for (rank = 1; rank < ntasks; rank += 1)
    {
      MPI_Send(buffer, 11, MPI_DOUBLE, rank, WORKTAG, MPI_COMM_WORLD);
    }
    


   /* Do the master work */
  if (type == 0)
    {
      smallOut = Mandelbrot(x_min, x_max, y_min, y_max, resolution, MAX_STEPS, width, newHeight);
    }
  else
    {
      smallOut = Julia(x_min, x_max, y_min, y_max, resolution, MAX_STEPS, complex_a, complex_b, width, newHeight);
    }

  for (i = 0; i < newHeight; i += 1)
    {
      for (j = 0; j < width; j += 1)
	{
	  image[i][j] = smallOut[i * width + j];
	}
    }
    

  /* There is no more work to be done so receive the results from the slaves */
  for (rank = 1; rank < ntasks; rank += 1)
    {
      MPI_Recv(smallOut, newHeight * width, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      /* Compute the line in the big matrix where this chunk will fit */
      startLine = status.MPI_SOURCE * newHeight;

        if (status.MPI_SOURCE != ntasks - 1)
	  {	
	    for (i = 0, k = startLine; i < newHeight; i += 1, k += 1)
	      {
	        for (j = 0; j < width; j += 1)
		  {
		    image[k][j] = smallOut[i * width + j];
		  } 
	      }
	  } 
          else
	   {
	     for (i = 0, k = startLine; i < difference; i += 1, k += 1)
	       {
	         for (j = 0; j < width; j += 1)
		   {
		     image[k][j] = smallOut[i * width + j];
		   }
	       }
	   }
       

    }
  /* Tell all the slaves to exit by sending an empty message with the DIETAG */
  for (rank = 1; rank < ntasks; rank += 1)
    {
      MPI_Send(image[0], 1, MPI_INT, rank, DIETAG, MPI_COMM_WORLD);
    }

  writePGM(outputName, width, height, image);

}

static void
slave(void)
{
  int type, width, height, MAX_STEPS, returnType, writeOut, startLine, myrank, i;
  double x_min, x_max, y_min, y_max, resolution, newy_min, complex_a, complex_b;
  int *image;
  double buffer[200];
  MPI_Status status;
  
  while (1)
    {
      /* Receive a message from the master */
      MPI_Recv(buffer, 11, MPI_DOUBLE, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      
      /* Check the tag of the received message */
      if (status.MPI_TAG == DIETAG)
	{
	  return;
	}

      /* Do the work */ 

      type = buffer[0];
      x_min = buffer[1];
      x_max = buffer[2];
      y_min = buffer[3];
      y_max = buffer[4];
      resolution = buffer[5];
      MAX_STEPS = buffer[6];
      width = buffer[7];
      height = buffer[8];
      complex_a = buffer[9];
      complex_b = buffer[10];

      MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
  
      startLine = myrank * height;
      newy_min = y_min;
      
      for (i = 0; i < startLine; i += 1)
	{
	  newy_min += resolution;
	}	

      if (type == 0)
	{
	  image = Mandelbrot(x_min, x_max, newy_min, y_max, resolution, MAX_STEPS, width, height);
	}
      else
	{
	  image = Julia(x_min, x_max, newy_min, y_max, resolution, MAX_STEPS, complex_a, complex_b, width, height);
	}

      /* Send the result back */
      MPI_Send(image, height * width, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
}

int *
Mandelbrot(double x_Min, double x_max, double y_Min, double y_Max, double res, int nrSteps, int w, int h)
{
  double cx, cy;
  double zx, zy, new_zx;
  int step, nx, ny, i, j, color;

  int *image = malloc(w * h * sizeof(int));
  
  cy = y_Min;
  for (i = 0; i < h; i += 1)
    {
      cx = x_Min;
      for (j = 0; j < w; j += 1)
	{
	  zx = 0.0;
	  zy = 0.0;
	  step = 0;
	  while ((zx * zx + zy * zy < 4) && (step < nrSteps))
	    {
	      new_zx = zx * zx - zy * zy + cx;
	      zy = 2.0 * zx * zy + cy;
	      zx = new_zx;
	      step += 1;
	    }
	  color = step % NUM_COLOR;
	  image[i * w + j] = color;
	  cx += res;
	}
      cy += res;
    }

  return image;
}

int *
Julia(double x_min, double x_max, double y_min, double y_max, double res, int nrSteps, double cRe, double cIm, int width, int height)
{
  double newzx, newzy, oldzx, oldzy, zx, zy;
  int i, j, step, color;
  
  int *image = malloc(width * height * sizeof(int));
  
  zy = y_min;
  for (i = 0; i < height; i += 1)
    {
      zx = x_min;
      for (j = 0; j < width; j += 1)
	{
	  step = 0;
	  oldzx = zx;
	  oldzy = zy;
	  while ((oldzx * oldzx + oldzy * oldzy < 4) && (step < nrSteps))
	    {
	      newzx = oldzx * oldzx - oldzy * oldzy + cRe;
	      newzy = 2 * oldzx * oldzy + cIm;
	      oldzx = newzx;
	      oldzy = newzy;
	      step += 1;
	    }
	  color = step % NUM_COLOR;
	  image[i * width + j] = color;
	  zx += res;
	}
      zy += res;
    }

  return image;
}

