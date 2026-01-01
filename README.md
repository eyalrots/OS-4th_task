# Memory Management Unit (MMU)

## Variables:

### 1. Valid array
This is an array of size $N$ which values are True / False.
Each index is a `page`, for each `page` we have True-valid / False-not valid.
* Valid: The `page` is in memory.
* Not Valid: The `page` needs to be fetched from the `Hard Disk`.

### 2. Dirty / Clean
This is an array sized $N$ which values are True / False.
Each index is a `page`, for each `page` we have True-dirty / False-clean.
* Dirty: The `page` in memory is different from the same `page` in disk.
* Clean: The `page` is the same both in memory and disk.
If the `page` is dirty the `Evicter` will have to write it to disk.

### 3. Reference
This is an array of size $N$ which values are 1 / 0.
Each index is a `page`, for each `page` we have 1-got used / 0-not used.
* 1: This means the page got a HIT in the wrire / read operation recently.
* 0: This means the page was not read or got written to.
This help the `Evicter` decide if the `page` deserves a second chance.

## Threads:

### 1. Main Thread
* Receives requests from Processes 1 and 2.
* Decide a Hit or Miss with probability $0 < \mathbb{P} < 1$.
* Actions:
    * HIT $\implies$ The `Reference bit` gets 1.
    * Read: imidiate Ack.
    * Write:
        * Sleep `MEM_WR_T` $ns$
        * Uniformly choose a page to mark `dirty`
        * Sends Ack.
    * Miss:
        * Check memory if full (all pages are `valid`)
            * Full -> Request `Evicter Thread` to make space.
        * Request `Hrad Disk Process` to read the page (make `valid` once got Ack from HD).
        * Perform the opreation requested (Read / Write) as stated above.
* Talks to:
    * Other threads:
        * Evicter
    * Other Processes:
        * Processes 1 and 2
        * Hard Disk

### 2. Evicter Thread
* Receives Requests from `Main Thread`
* Woken up only when memory is full - to make room.
* Eviction Algorithms:
    * Uses a circular index to choose evicted thread.
    * For each `page` check `Reference` bit:
        * 1 -> `page` gets a second chance, `Reference` bit gets 0 and not being evicted - move to another `page`.
        * 0 -> `page` is being evicted.
    * For this `page` with `Reference` 0 - check `dirty` bit:
        * True -> `page` is `dirty` and needs to be written to disk.
            * A request is made to the `Hard Disk` process.
            * Eviction algorithm continues on Ack from `Hard Disk`.
        * False -> `page` is clean and gets evicted imidiatly.
    * Check how many bits are `valid`, if $< N$ wake up `Main Thread`.
    * Continue this algorithm untill the are `USED_SLOTS_TH` bits that are `valid`.
* When the algorithm is done the thread goes back to sleep.
* Talks to:
    * Other Threads:
        * Main
    * Other Processes:
        * Hard Disk

### 3. Printer Thread
* At beginning of the simulation and every `TIME_BETWEEN_SNAPSHOTS` $ns$ outputs a snapshot of memory.
* Snapshot details for each `page`:
    * 0: `page` is `valid` and `clean`.
    * 1: `page` is `valid` and `dirty`.
    * -: `page` is `invalud`.
* This operation should act like a CS in the MMU - no reads / writes.
* Two empty lines must seperate every seperate snapshots.
* Critical section copies bit data to local variables then releases mutex - minimize CS time.
