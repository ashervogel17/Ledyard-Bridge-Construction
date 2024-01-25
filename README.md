# Ledyard-Bridge-Construction

## Solution

### Structs

Structs are used to maintain logical unity.

#### Bridge

The bridge struct represents the bridge between Hanover and Norwich. One bridge struct is declared as a global. Memory for the bridge is allocated and freed from main, and it is used thoughout many of the function described below. The bridge struct has the following attributes:

- max_num_cars: maximum number of cars allowed on the bridge at once (integer)
- current_num_cars: current number of cars on the bridge (integer)
- direction: determines whether the traffic is currently flowing to Norwich (0) or Hanover (1) (integer)
- lock: grants one thread at a time access to read or write from the bridge (pthread_mutex_t)
- enter_to_hanover_cvar: condition variable for cars waiting to cross to Hanover (pthread_cond_t)
- enter_to_norwich_cvar: condition variable for cars waiting to cross to Norwich (pthread_cond_t)
- exit_bridge_cvar: condition variable for cars waiting to exit the bridge (pthread_cond_t)

#### Car

The car struct represents a car waiting to cross, currently crossing, or done crossing the bridge. It has the following attributes:

- thread_id: index of the thread driving this car, mainly used for printing (integer)
- direction: 0 if the car is going to Norwich, 1 if going to Hanover (integer)

### Functions

- ```int validate_params(int argc, char* argv[])```
- ```int get_direction(int direction, char* buffer)```
- ```bridge_t* new_bridge(int max_num_cars)```
- ```car_t* new_car(int thread_id, int direction)```
- ```void on_bridge(car_t* car)```
- ```void acquire_bridge(car_t* car)```
- ```void exit_bridge(car_t* car)```
- ```void handle_car(car_t* car)```

### Synchronization

The synchronization is primarily handled by ```acquire_bridge``` and ```exit_bridge```. It relies on the core principle that a thread can only read from or write to the central bridge struct if it holds the mutex. This ensures that no two threads are attempting to access the shared data in the bridge struct at the same time.

At the start of ```acquire_bridge```, the current thread attempts to acquire the mutex. If not successful, it uses the cvar corresponding to the direction it's trying to go to wait for the lock to be available. Once it successfully acquires the lock, it checks that the bridge is neither full nor has cars travelling in the opposite direction. If either of these conditions are not met, the thread releases the lock and waits for it to be ready again. Notice that these read operations happen after the lock if acquired. 

Once the thread acquires the lock, (1) checks that the bridge direction matches its own and that the bridge is not full, returning non-zero if either condition is not met, (2) increments the bridge's current car count, (3) calls ```on_bridge``` to print the current state of the bridge, (4) releases the lock, (5) sleeps for 1 second to simulate crossing the bridge, (5) pokes the appropriate cvars (explained Liveness section). 

```exit_bridge``` similarly attempts to acquire the lock for the current thread, using a cvar to minimize spinning. Once the thread gets the lock, it decrements the bridge's car count and pokes the appropriate cvars.

### Safety

When a car wants to get on the bridge, it must first acquire the lock. However, if after acquiring the lock, the thread sees that the bridge capacity is full, it releases the lock and gets back in line to acquire the lock again. Similarly, if the thread sees that the bridge is non-empty and traffic is flowing in the wrong direction, it relinquishes the lock and waits for it again. If a car is the first one on the bridge, it makes sure to set the bridge direction to its own.

With this logic in place, there should not be any collisions or crashes. For good measure, these conditions are regularly checked and the program exits nonzero if there is a failure.

### Liveness

The liveness problem is solved by the way in which cvars are signaled.

When a car is done entering the bridge, it checks to see if the bridge is at capacity. If so, it pokes the exiting cvar only. If there is capacity, it pokes the cvar for cars waiting to enter in the same direction before poking the exiting cvar.

When a car finishing exiting the bridge, it checks to see if it was the last car off the bridge. If it was, it first pokes the cvar for cars to get on in the opposite direction before poking cars to get on in the same direction. If there are cars on the bridge behind the car that just exited, it first signals other cars travelling in the same direction to enter the bridge before signaling the exiting cvar.

With this logic in place, there shouldn't be any liveness issues. This is confirmed by testing the program with various cases and random car orientation.

## Usage

```
gcc -o ./bridge driver.c
./bridge <TOTAL_NUM_CARS> <MAX_CARS_ON_BRIDGE>
```

## Testing

```
sh testing.sh
```

## Acknowledgments

Both Alexander Chen and Luc Cote (TA's) were really great in helping understand how to break the problem down and come up with a solution.