#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define DIRECTION_TO_NORWICH 0
#define DIRECTION_TO_HANOVER 1

// Return 0 iff argc = 3 and argv[1] and arv[2] are integers
// Returns -1 with error message otherwise
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

// Copy string representation of direction into buffer and return 0 if direction is 0 or 1
// Copies empty string into buffer and returns -1 if direction is neither 0 nor 1
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

// Structure to represent bridge
typedef struct bridge {
  int max_num_cars;
  int current_num_cars;
  int direction;
  pthread_mutex_t lock;
  pthread_cond_t enter_to_hanover_cvar;
  pthread_cond_t enter_to_norwich_cvar;
  pthread_cond_t exit_bridge_cvar;
} bridge_t;

// Structure to represent a car crossing the bridge
typedef struct car {
  int thread_id;
  int direction;
} car_t;

// Allocate memory for a new bridge structure and intialize attributes.
// Caller is responsible for calling free(bridge)!
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
  bridge->exit_bridge_cvar = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
}

// Allocate memory for a new car structure and intialize attributes.
// Caller is responsible for calling free(car)!
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

// Global variable to store bridge structure
// Memory is allocated and freed from main()
bridge_t* bridge;

// Call this function when a car gets on the bridge
// Caller is expected to have the bridge's mutex
void on_bridge(car_t* car) {
  // Double check that bridge direction matches current car direction
  if (bridge->direction != car->direction) {
    printf("Crossing car's direction does not match bridge's direction.\n");
    exit(1);
  }

  // Print state of current car and bridge
  int car_id = car->thread_id;
  char car_direction[20];
  get_direction(car->direction, car_direction);
  int num_cars = bridge->current_num_cars;
  char bridge_direction[20];
  get_direction(bridge->direction, bridge_direction);
  printf("Car #%d is on the bridge going %s. There are %d cars on the bridge going %s.\n", car_id, car_direction, num_cars, bridge_direction);
  fflush(stdout);
}

// Current threads attempt to lock the bridge mutex.
// If the bridge is full or traffic is flowing the wrong way, the mutex is unlocked and caller tries again.
// Once the mutex is acquired and the bridge is safe for crossing, 
//  increment the bridge car count and make sure the direction is set correctly
// Call on_bridge()
// Release the mutex
// Sleep for 1 second to simulate crossing the bridge
// Signal any threads that are waiting to get on the bridge if the bridge is not full
// Signal any threads waiting to get off
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
  int can_enter_bridge = 0;
  while (can_enter_bridge == 0) {
    while (pthread_mutex_lock(&(bridge->lock)) != 0) {
      // Wait for cvar to receive signal
      pthread_cond_wait(&enter_cvar, &(bridge->lock));
    }
    // Lock acquired
    // If bridge is full, release lock and try again
    if (bridge->current_num_cars >= bridge->max_num_cars) {
      pthread_mutex_unlock(&(bridge->lock));
    }
    // If bridge has cars moving in opposite direction, release lock and try again
    else if (bridge->current_num_cars > 0 && bridge->direction != car->direction) {
      pthread_mutex_unlock(&(bridge->lock));
    }
    // Otherwise, move on to board the bridge
    else {
      can_enter_bridge = 1;
    }
  }
  
  // Double-check that conditions are not broken. Print out if conditions are violated.
  if (bridge->current_num_cars > bridge->max_num_cars) {
    printf("The bridge just collapsed :( \n");
    exit(1);
  }
  if (bridge->direction != car->direction) {
    // If car is the first onto the bridge, make sure bridge direction is set correctly.
    if (bridge->current_num_cars == 0) {
      bridge->direction = car->direction;
    }
    else {
      printf("Head-on collision just happended :( \n");
      exit(1);
    }
  }

  // Increment bridge current car count
  bridge->current_num_cars++;

  // Print status
  on_bridge(car);
  printf("Car %d entered bridge\n", car->thread_id);
  
  // Release the lock
  pthread_mutex_unlock(&(bridge->lock));

  // Sleep to simulate crossing the bridge
  sleep(1);

  // If bridge is full, only wake up cars waiting to exit
  // Otherwise, wake up a car going in the same direction, followed by the exit cvar
  if (bridge->current_num_cars < bridge->max_num_cars) {
    pthread_cond_signal(&enter_cvar);
  }
  pthread_cond_signal(&(bridge->exit_bridge_cvar));
}

// Current thread blocks until it can acquire the mutex
// Once the lock is acquired, decrement the car count, print out that the car is leaving, then:
// If the bridge is empty, signal to both sides of the bridge that they can get on.
// Whoever gets the lock first will set the direction so that the other side doesn't get on
// If the bridge is not empty, signal cars waiting on the side we came from to gt on, then signal cars waiting to exit
void exit_bridge(car_t* car) {
  // Aquire lock to modify bridge state
  while(pthread_mutex_lock(&(bridge->lock)) != 0) {
    // Wait for cvar to receive signal
    pthread_cond_wait(&(bridge->exit_bridge_cvar), &(bridge->lock));
  }

  // Double-check that the bridge is not already empty
  if (bridge->current_num_cars == 0) {
    printf("You're trying to exit a car when the bridge is already empty!\n");
    exit(1);
  }

  // Double-check that bridge direction matches car's direction
  if (bridge->direction != car->direction) {
    printf("Exiting car's direction does not match bridge's direction.\n");
    exit(1);
  }

  // Decrement bridge's current car count
  bridge->current_num_cars--;

  printf("Car %d exiting bridge\n", car->thread_id);

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
    pthread_cond_signal(&opp_dir_cvar);
    pthread_cond_signal(&same_dir_cvar);
  }
}

// Wrapper to handle all car functions
void handle_car(car_t* car) {
  acquire_bridge(car);
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