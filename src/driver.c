#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define DIRECTION_TO_NORWICH 0
#define DIRECTION_TO_HANOVER 1

int validate_params(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Invalid Usage. Correct usage:\n%s <number of cars> <max cars on bridge>\n", argv[0]);
    return -1;
  }

  int num_cars = atoi(argv[1]);
  if (num_cars == 0 && argv[1][0] != '0') {
    fprintf(stderr, "Error. Number of cars must be an integer, got %s\n", argv[1]);
    return -1;
  }

  int max_num_cars = atoi(argv[2]);
  if (max_num_cars == 0 && argv[2][0] != '0') {
    fprintf(stderr, "Error. Maximum number of cars must be an integer, got %s\n", argv[2]);
    return -1;
  } 

  return 0;
}

int get_direction(int direction, char* buffer) {
  if (direction == DIRECTION_TO_HANOVER) {
    strcpy(buffer, "to Hanover");
  }
  else if (direction == DIRECTION_TO_NORWICH) {
    strcpy(buffer, "to Norwich");
  }
  else {
    fprintf(stderr, "Invalid direction %d", direction);
    strcpy(buffer, "");
    return -1;
  }
  return 0;
}

typedef struct bridge {
  int max_num_cars;
  int current_num_cars;
  int direction;
  pthread_mutex_t lock;
  pthread_cond_t enter_to_hanover_cvar;
  pthread_cond_t enter_to_norwich_cvar;
  pthread_cond_t exit_bridge_cvar;
} bridge_t;


typedef struct car {
  int thread_id;
  int direction;
} car_t;

bridge_t* bridge;


bridge_t* new_bridge(int max_num_cars) {
  bridge_t* bridge = (bridge_t*) malloc(sizeof(bridge_t));
  if (bridge == NULL) {
    fprintf(stderr, "Error allocating memory for new bridge.\n");
    return NULL;
  }
  bridge->max_num_cars = max_num_cars;
  bridge->current_num_cars = 0;
  bridge->direction = rand() % 2;
  bridge->lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
  bridge->enter_to_hanover_cvar = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
  bridge->enter_to_norwich_cvar = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
}

// Caller must free memory!
car_t* new_car(int thread_id, int direction) {
  car_t* car = (car_t*) malloc(sizeof(car_t));
  if (car == NULL) {
    fprintf(stderr, "Error allocating memory for new car.\n");
    return NULL;
  }
  car->thread_id = thread_id;
  car->direction = direction;
  return car;
}

void acquire_bridge(car_t* car) {
  // Get reference to the cvar that matches this car's direction
  pthread_cond_t enter_cvar;
  if (car->direction == DIRECTION_TO_HANOVER) {
    enter_cvar = bridge->enter_to_hanover_cvar;
  }
  else {
    enter_cvar = bridge->enter_to_norwich_cvar;
  }

  // Aquire lock to modify bridge state
  while (pthread_mutex_lock(&(bridge->lock)) != 0) {
    // Wait for cvar to receive signal
    pthread_cond_wait(&enter_cvar, &(bridge->lock));
  }
  
  // Double-check that conditions are not broken.
  if (bridge->current_num_cars > bridge->max_num_cars) {
    printf("The bridge just collapsed :( \n");
  }
  if (bridge->direction != car->direction) {
    // If car is the first onto the bridge, make sure bridge direction is set correctly.
    if (bridge->current_num_cars == 0) {
      bridge->direction = car->direction;
    }
    else {
      printf("Head-on collision just happended :( \n");
    }
  }

  // Increment bridge current car count
  bridge->current_num_cars++;

  // Release the lock
  pthread_mutex_unlock(&(bridge->lock));

  // If bridge is full, only wake up cars waiting to exit
  // Otherwise, wake up a car going in the same direction, followed by the exit cvar
  if (bridge->current_num_cars < bridge->max_num_cars) {
    pthread_cond_signal(&enter_cvar);
    
  }
  pthread_cond_signal(&(bridge->exit_bridge_cvar));
}

void on_bridge(car_t* car) {
  // Double check that bridge direction matches current car direction
  if (bridge->direction != car->direction) {
    printf("Crossing car's direction does not match bridge's direction.\n");
  }

  // Print state of current car and bridge
  int car_id = car->thread_id;
  char car_direction[20];
  get_direction(car->direction, car_direction);
  int num_cars = bridge->current_num_cars;
  char bridge_direction[20];
  get_direction(bridge->direction, bridge_direction);
  printf("Car #%d is on the bridge going %s. There are %d cars on the bridge going %s.\n", car_id, car_direction, num_cars, bridge_direction);
}

void exit_bridge(car_t* car) {
  // Aquire lock to modify bridge state
  while(pthread_mutex_lock(&(bridge->lock)) != 0) {
    // Wait for cvar to receive signal
    pthread_cond_wait(&(bridge->exit_bridge_cvar), &(bridge->lock));
  }

  // Double-check that the bridge is not already empty
  if (bridge->current_num_cars == 0) {
    printf("You're trying to exit a car when the bridge is already empty!\n");
  }

  // Double-check that bridge direction matches car's direction
  if (bridge->direction != car->direction) {
    printf("Exiting car's direction does not match bridge's direction.\n");
  }

  // Decrement bridge's current car count
  bridge->current_num_cars--;

  // Release mutex
  pthread_mutex_unlock(&(bridge->lock));

  // Get reference to cvar that corresponds to same and opposite direction
  pthread_cond_t same_dir_cvar;
  pthread_cond_t opp_dir_cvar;
  if (car->direction == DIRECTION_TO_HANOVER) {
    same_dir_cvar = bridge->enter_to_hanover_cvar;
    opp_dir_cvar = bridge->enter_to_norwich_cvar;
  }
  else {
    same_dir_cvar = bridge->enter_to_norwich_cvar;
    opp_dir_cvar = bridge->enter_to_hanover_cvar;
  }

  // If the bridge still has cars on it, wake up same direction cvar, followed by exit cvar.
  if (bridge->current_num_cars > 0) {
    pthread_cond_signal(&same_dir_cvar);
    pthread_cond_signal(&(bridge->exit_bridge_cvar));
  }

  // If the bridge is empty, wake up opposite direction cvar, followed by same direction cvar
  else {
    printf("Signaling next car\n");
    pthread_cond_signal(&opp_dir_cvar);
    pthread_cond_signal(&same_dir_cvar);
  }
}

void handle_car(car_t* car) {
  acquire_bridge(car);
  on_bridge(car);
  exit_bridge(car);
}


int main(int argc, char* argv[]) {
  // Validate parameters
  int status = validate_params(argc, argv);
  if (status != 0) {
    return status;
  }
  
  // Store parameters as integers on the stack
  const int starting_num_cars = atoi(argv[1]);
  const int max_cars_on_bridge = atoi(argv[2]);

  // Allocate memory for the bridge
  bridge = new_bridge(max_cars_on_bridge);
  if (bridge == NULL) {
    fprintf(stderr, "Error allocating memory for bridge.\n");
    return -1;
  }

  // Allocate memory for array of cars
  car_t** cars = (car_t**) malloc(starting_num_cars*sizeof(car_t*));
  if (cars == NULL) {
    free(bridge);
    fprintf(stderr, "Error allocating memory for array of cars.\n");
    return -1;
  }

  // Allocate memory for each car
  int thread_id, direction;
  for (int i = 0; i < starting_num_cars; i++) {
    thread_id = i;
    direction = rand() % 2;
    cars[i] = new_car(thread_id, direction);
    if (cars[i] == NULL) {
      for (int j = 0; j < i; j++) {
        free(cars[j]);
      }
      free(bridge);
      free(cars);
      fprintf(stderr, "Error allocating memory for cars. Exiting.\n");
      return -1;
    }
  }

  /////// DEBUG
  for (int i = 0; i < starting_num_cars; i++) {
    if (cars[i]->direction == DIRECTION_TO_HANOVER) {
      printf("to hanover\n");
    }
    else {
      printf("to norwich\n");
    }
  }

  //////

  // Kick off a thread for each car
  pthread_t threads[starting_num_cars];
  for (int i = 0; i < starting_num_cars; i++) {
    status = pthread_create(&threads[i], NULL, (void*) handle_car, cars[i]);
    // Handle error when creating thread
    if (status != 0) {
      for (int j = 0; j < i; j++) {
        pthread_join(threads[j], NULL);
      }
    }
  }
  
  // Clean up
  for (int i = 0; i < starting_num_cars; i++) {
    pthread_join(threads[i], NULL);
  }
  for (int i = 0; i < starting_num_cars; i++) {
    free(cars[i]);
  }
  free(cars);
  free(bridge);
  return 0;
}