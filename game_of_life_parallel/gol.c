
/*
 * To run:
 * ./gol file1.txt  0  # run with config file file1.txt, do not print board
 * ./gol file1.txt  1  # run with config file file1.txt, ascii animation
 * ./gol file1.txt  2  # run with config file file1.txt, ParaVis animation
 *
 */
#include <pthreadGridVisi.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "colors.h"

/****************** Definitions **********************/
/* Three possible modes in which the GOL simulation can run */
#define OUTPUT_NONE   (0)   // with no animation
#define OUTPUT_ASCII  (1)   // with ascii animation
#define OUTPUT_VISI   (2)   // with ParaVis animation

/* Used to slow down animation run modes: usleep(SLEEP_USECS);
 * Change this value to make the animation run faster or slower
 */
//#define SLEEP_USECS  (1000000)
#define SLEEP_USECS    (100000)

/* A global variable to keep track of the number of live cells in the
 * world
 */
static int total_live = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrier;


//Update global vars for Parallel mode
int num_threads;

/* This struct represents all the data we need to keep track in our GOL
 * simulation.
 */
struct gol_data {

    int rows;  // the row dimension
    int cols;  // the column dimension
    int start; 
    int end;
    int iters; // number of iterations to run the gol simulation
    int output_mode; // set to:  OUTPUT_NONE, OUTPUT_ASCII, or OUTPUT_VISI
    int * cells;
    int * next_cells;
    int partition;
    int print_mode;
    int tid;

    int current_round;

    /* fields used by ParaVis library (when run in OUTPUT_VISI mode). */
    visi_handle handle;
    color3 *image_buff;
};

/************ Definitions for using ParVisi library ***********/
/* initialization for the ParaVisi library (DO NOT MODIFY) */
int setup_animation(struct gol_data* data);
/* register animation with ParaVisi library (DO NOT MODIFY) */
int connect_animation(void (*applfunc)(struct gol_data *data),
        struct gol_data* data);
/* name for visi (you may change the string value if you'd like) */
static char visi_name[] = "GOL!";







/****************** Function Prototypes **********************/
/* the main gol game playing loop (prototype must match this) */
void *play_gol(void *args);

/* init gol data from the input file and run mode cmdline args */
int init_game_data_from_args(struct gol_data *data, char **argv);

// A mostly implemented function, but a bit more for you to add.
/* print board to the terminal (for OUTPUT_ASCII mode) */
void print_board(struct gol_data *data, int round);

// Return a list of neighbors of a given cell (represented by 1 or 0)
int get_neighbors(struct gol_data *data, int i, int j);
int update_world(struct gol_data *data, int neighbors, int i, int j);





//                                          WORK ZONE
//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
int main(int argc, char **argv) {

    int ret;
    int stride;
    struct gol_data data;
    double secs;
    struct timeval start_time, stop_time;

    /* check number of command line arguments */
    if (argc != 6) {
        printf("usage: %s <infile.txt> <output_mode>[0|1|2] <num_threads> <partition>[0,1] <print_partition>[0,1]\n", argv[0]);
        exit(1);
    }

    /* Initialize game state (all fields in data) from information
     * read from input file */
    ret = init_game_data_from_args(&data, argv);
    if (ret != 0) {
        printf("Initialization error: file %s, mode %s\n", argv[1], argv[2]);
        exit(1);
    }

    num_threads = atoi(argv[3]);
    data.partition = atoi(argv[4]);
    data.print_mode = atoi(argv[5]);

    int remainder;

    //print error messages if # threads > #rows/cols
    //then calculate stride of partition.

    if (data.partition==1){ // cols partition
        if (num_threads > data.cols) {
            printf("Error: More threads than number of columns.");
            exit(0);
        }
        stride = data.cols/num_threads; printf("Stride: %d\n",stride);
        remainder = data.cols%num_threads; printf("Remainder: %d\n",remainder);
    }
    else if (data.partition==0){ //rows partition
        if (num_threads > data.rows) {
            printf("Error: More threads than number of rows.");
            exit(0);
        }
        stride = data.rows/num_threads; 
        
        remainder = data.rows%num_threads; 
    }

    pthread_t *threads;
    pthread_barrier_init(&barrier, NULL, num_threads);

    threads = malloc(num_threads * sizeof(pthread_t));
    data.next_cells = malloc(data.rows*data.cols * sizeof(int));

    //initialize all cells in next_cells to 0;
    for (int i = 0; i < data.rows*data.cols; i++) {
        data.next_cells[i] = 0;
    }



    struct gol_data* thread_data = malloc(num_threads * sizeof(struct gol_data));


    ret = gettimeofday(&start_time, NULL);

    ===========================================
    /*  THREADING SECTION */
    for (int i = 0; i < num_threads; i++) {


        thread_data[i] = data;
        thread_data[i].tid = i;

        //Partition for cases with remainder == 0;
        if (remainder == 0) {

            thread_data[i].start = i*stride;
            thread_data[i].end = (i+1)*stride;      

            pthread_create(&threads[i], NULL, play_gol, &thread_data[i]);
        }

        //Partition for cases with remainder != 0;
        else {
            
            if (i==0) {
                thread_data[i].start = 0;
                thread_data[i].end = stride;
            }
            else {
                thread_data[i].start = thread_data[i-1].end;
                thread_data[i].end = thread_data[i].start + stride;
            }


            if (i < remainder) {
                thread_data[i].end +=1;
            }

            pthread_create(&threads[i], NULL, play_gol, &thread_data[i]);

        }
    }

    //needs to be done before thread join
    /* initialize ParaVisi animation (if applicable) */
    if (data.output_mode == OUTPUT_VISI) {
        setup_animation(&data);
    }

====================================================

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    

    /* ASCII output: clear screen & print the initial board */
    // if (data.output_mode == OUTPUT_ASCII) {
    //     if (system("clear")) { perror("clear"); exit(1); }
    //     print_board(&data, 0);
    // }

    //starts timer


    /* Invoke play_gol in different ways based on the run mode */
    // if (data.output_mode == OUTPUT_NONE) {  // run with no animation
    //     // play_gol(&data);
    // }
    // else 

    if (data.output_mode == OUTPUT_ASCII) { // run with ascii animation
        // play_gol(&data);

        // clear the previous print_board output from the terminal:
        // (NOTE: you can comment out this line while debugging)
        if (system("clear")) { perror("clear"); exit(1); }

        // NOTE: DO NOT modify this call to print_board at the end
        //       (it's to help us with grading your output)
        print_board(&data, data.iters);
    }
    // else {  // OUTPUT_VISI: run with ParaVisi animation
    //         // tell ParaVisi that it should run play_gol

    //     // connect_animation(play_gol, &data);

    //     // start ParaVisi animation

    //     // run_animation(data.handle, data.iters);
    // }


    //stops the timer right before printing final lines
    ret = gettimeofday(&stop_time, NULL);


    //calculates time elapsed since start of program
    secs = stop_time.tv_sec - start_time.tv_sec;
    secs +=  ((double)stop_time.tv_usec - (double)start_time.tv_usec)/1000000;


    if (data.output_mode != OUTPUT_VISI) {
        /* Print the total runtime, in seconds. */
        fprintf(stdout, "Total time: %0.3f seconds\n", secs);
        fprintf(stdout, "Number of live cells after %d rounds: %d\n\n",
                data.iters, total_live);
    }

    free(data.cells);
    free(threads);

    return 0;
}





/* initialize the gol game state from command line arguments
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode value
 * data: pointer to gol_data struct to initialize
 * argv: command line args
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode
 * returns: 0 on success, 1 on error
 */
int init_game_data_from_args(struct gol_data *data, char **argv) {
    FILE * infile;
    int ret;

    infile = fopen(argv[1], "r");
    if (infile == NULL) {
        printf("Error: Failure to open file.\n");
        return 1;
    }

    /*Reads in output mode from command-line.
    Sets current round to 0*/
    data->output_mode = atoi(argv[2]);
    data->current_round = 0;

    /*Reads in number of rows, columns, iterations, and
    initially alive cells from txt file.*/
    ret = fscanf(infile, "%d", &data->rows);
        if (ret == 0) {return 1;}
    ret = fscanf(infile, "%d", &data->cols);
        if (ret == 0) {return 1;}
    ret = fscanf(infile, "%d", &data->iters);
        if (ret == 0) {return 1;}
    ret = fscanf(infile, "%d", &total_live);
        if (ret == 0) {return 1;}

    //Allocating space based on size of the 2D world.
    int num_cells = data->rows*data->cols;
    data->cells = malloc (num_cells * sizeof(int));

    //Set all cells to 0 (dead).
    for (int i = 0; i < num_cells; i++) {
        data->cells[i] = 0;
    }

    //Let x be row coordinate, y be column coordinate of any cell.
    int x, y;
    //Set alive cells to 1.
    for (int j = 0; j < total_live; j++) {

        ret = fscanf(infile, "%d %d", &x, &y);
        if (ret == 0) {return 1;}

        //convert cell's x-y coordinate to array's index.
        int idx = x*data->cols + y;
        data->cells[idx] = 1;
    }

    ret = fclose(infile);
    if (ret != 0) {
        printf("Error: Failure to close file.\n");
        return 1;
    }

    return 0;
}

/*Function to count number of neighbors that a given cell has.
Params: gol_data file, i-j coords of the cell.
Return: number of neighbors of a cell.*/
int count_neighbors(struct gol_data *data, int i, int j){
    //sets the number of alive neighbors to 0
    int neighbors = 0;
    int index_neighbor;
    int rows = data->rows;
    int cols = data->cols;

    /*checks each neighbor to see if they are alive and if so increments 
    the variable neighbors*/
    for (int l = -1; l <= 1; l++){
        for(int k = -1; k <= 1; k++){
            // index_neighbor = (abs(((i + l) % rows) * cols)) + abs((
            //     (j+k) % cols));       
            index_neighbor = (((((i+l) % rows) + rows) % rows) * cols) + 
            ((((j+k) % cols) + cols ) % cols);    
            if (data->cells[index_neighbor] == 1){
                neighbors++;
            }
        }
    }
    return neighbors;
}




/*Function to update data for next round without
affecting the data of current round.*/
int update_world(struct gol_data *data, int neighbors, int i, int j) {

    int idx = i*data->cols + j;
    int temp_cell = data->cells[idx];

    //If cell is currently alive
    if (temp_cell == 1) {
        //and if it has exactly 2 or 3 neighbors
        if (neighbors == 2 || neighbors == 3) {
            data->next_cells[idx] = 1;
            return 1;
        }
    }
    //If cell is not alive and has 3 neighbors
    else if (temp_cell == 0) {
        if (neighbors == 3){
            data->next_cells[idx] = 1;
            // total_live++;
            return 1;
        }
    }

    return  0;
}


/*Use data from the data->cells array to control 
color of pixels in VISI mode.
Modified from weekly lab meeting code.*/
void update_color(struct gol_data * data) {
    int i, j, rows, cols, idx, b;
    
    rows = data->rows;
    cols = data->cols;

    for (i=0; i < rows; i++) {
        for (j=0; j < cols; j++) {
            idx = i*cols + j;

            //Note: (0,0) is upper left on the board and lower left in the image buffer.
            b = (rows - (i+1))*cols + j;

            if (data->cells[idx] == 1) {
                data->image_buff[b] = c3_red;
            }
            else {
                data->image_buff[b] = c3_green;
            }
        }
    }
}



/* the gol application main loop function:
 *  runs rounds of GOL,
 *    * updates program state for next round (world and total_live)
 *    * performs any animation step based on the output/run mode
 *
 *   data: pointer to a struct gol_data  initialized with
 *         all GOL game playing state
 */
void *play_gol(void *args) {

    struct gol_data *data;
    data = (struct gol_data *)args;
    int pt_TLC = 0; //per thread total live cells

    int neighbors, temp_cell;
    // int num_cells = data->rows * data->cols;

    if(data->print_mode==1 && data->partition==0) {
        printf("TID: %d; rows: %d --> %d (%d); cols: 0 --> %d (%d)\n", data->tid, data->start, data->end-1, 
        data->end-data->start, data->cols-1, data->cols);
    }
    if(data->print_mode==1 && data->partition==1) {
        printf("TID: %d; rows: 0 --> %d (%d); cols: %d --> %d (%d)\n", data->tid, data->rows-1,
        data->rows, data->start, data->end-1, data->end-data->start);
    }

    // return NULL;

    for (int k = 0; k < data->iters; k++) {

        pt_TLC = 0;
        total_live = 0;

        if(data->partition == 0){
            for (int i = data->start; i < data->end; i++) {
                for (int j = 0; j < data->cols; j++) {
                    int idx = i*data->cols + j;
                    neighbors = count_neighbors(data, i, j);
                    temp_cell = data->cells[idx];
                    neighbors -= temp_cell;
                    pt_TLC+= update_world(data, neighbors, i, j);
                }
            }
        }
        else
        {
            for (int i = 0; i < data->rows; i++) {
                for (int j = data->start; j < data->end; j++) {
                    int idx = i*data->cols + j;
                    neighbors = count_neighbors(data, i, j);
                    temp_cell = data->cells[idx];
                    neighbors -= temp_cell;
                    pt_TLC+= update_world(data, neighbors, i, j);
                }
            }  
        }

        pthread_barrier_wait(&barrier);
        pthread_mutex_lock(&mutex);

        total_live+=pt_TLC;


        pthread_mutex_unlock(&mutex);


        int * temp_array;
        temp_array = data->cells;
        data->cells = data->next_cells;
        data->next_cells = temp_array;

        data->current_round++;
        
    }
    //frees the heap memory used by the temporary array
    // free(data->next_cells); //invalid free
    return NULL;
}




/* Print the board to the terminal.
 *   data: gol game specific data
 *   round: the current round number
 */
void print_board(struct gol_data *data, int round) {

    int i, j;
    /* Print the round number. */
    fprintf(stderr, "Round: %d\n", data->current_round);
    for (i = 0; i < data->rows; ++i) {
        for (j = 0; j < data->cols; ++j) { 

            //Convert 2D world's i-j coordinates to array's index.     
            int idx = i*data->cols + j;

            //If cell is alive, prints '@', if not prints '.'
            if (data->cells[idx]==1) {
                fprintf(stderr, " @");
            }
            else {
                fprintf(stderr, " .");
            }
        }
        fprintf(stderr, "\n");
    }

    /* Print the total number of live cells. */
    fprintf(stderr, "Live cells: %d\n\n", total_live);
}

//$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$









/**********************************************************/
/***** DO NOT MODIFY *****/
/* initialize ParaVisi animation */
int setup_animation(struct gol_data* data) {
    /* connect handle to the animation */
    int num_threads = 1;
    data->handle = init_pthread_animation(num_threads, data->rows,
            data->cols, visi_name);
    if (data->handle == NULL) {
        printf("ERROR init_pthread_animation\n");
        exit(1);
    }
    // get the animation buffer
    data->image_buff = get_animation_buffer(data->handle);
    if(data->image_buff == NULL) {
        printf("ERROR get_animation_buffer returned NULL\n");
        exit(1);
    }
    return 0;
}

/* sequential wrapper functions around ParaVis library functions */
void (*mainloop)(struct gol_data *data);

void* seq_do_something(void * args){
    mainloop((struct gol_data *)args);
    return 0;
}

int connect_animation(void (*applfunc)(struct gol_data *data),
        struct gol_data* data)
{
    pthread_t pid;

    mainloop = applfunc;
    if( pthread_create(&pid, NULL, seq_do_something, (void *)data) ) {
        printf("pthread_created failed\n");
        return 1;
    }
    return 0;
}
/***** DO NOT MODIFY *****/
