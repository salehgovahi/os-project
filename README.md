# Multi-thread and multi-process Shopping System

## Overview

The Multi-thread and multi-process Shopping Systemis a midterm project designed for the Operating Systems course. This application allows users to interact with multiple virtual stores, managing orders and product information in a concurrent environment. The project showcases important operating system concepts such as processes, threads, shared memory, and synchronization mechanisms.
Users can place orders, receive suggestions based on their inputs, and see real-time updates on product availability, pricing, scores, and other crucial details.

## Features

- **Dynamic Loading of Stores and Products:** The application loads store data, including categories and products, from a specified dataset directory.
- **Multi-threading and Multi-processing:** Utilizes POSIX threads (pthread) and forked processes to manage user interactions, product processing, order handling, and finalization concurrently.
- **Shared Memory Management:** Employs shared memory to facilitate communication between different processes and threads, ensuring data consistency.
- **Order Management:** Supports users in creating orders, including the ability to specify product quantities and set a price threshold.
- **Score Management:** Users can provide ratings for products, affecting their scores and future suggestions.
- **Concurrency Control:** Utilizes mutexes to ensure thread-safe operations.

## Directory Structure

The application expects a specific directory structure for the dataset input, typically like this:

```
Dataset/
│
├── StoreA/
│   ├── Category1/
│   │   ├── product1.txt
│   │   ├── product2.txt
│   ├── Category2/
│       ├── product3.txt
│
├── StoreB/
│   ├── Category1/
│       ├── product1.txt
│       ├── product2.txt
│   ├── Category2/
│       ├── product3.txt
│       └── product4.txt
```

Each `productX.txt` file contains details of the product in the following format:
```
Name: Product Name
Price: 100.00
Score: 4.5
Entity: 10
Last Modified: 2023-01-01 12:00:00
```

## Compilation and Execution

1. **Compilation:** Use a C compiler (e.g., `gcc`) to compile the program. Ensure all necessary libraries (pthread, sys) are linked.

   ```bash
   gcc -o shopping_system main.c -lpthread
   ```

2. **Execution:** Run the program, while specifying the Dataset directory if needed. There are no particular command-line arguments expected.

   ```bash
   ./shopping_system
   ```

### Sample Interaction

Upon running, users will be prompted for inputs, including:

- Username
- Product orders (format: `product_name quantity`)
- Price threshold

The user can enter "exit" at any time to quit the program.

## Code Structure

The core functions of the application are organized into sections:

- **Data Loading:**
  - `load_dataset`: Loads stores, categories, and products from the given dataset directory into memory.

- **Processes and Threads:**
  - `user_process`: Manages individual user sessions.
  - `process_product`: Handles individual product processing for orders.
  - `process_orders`: Manages the collection of orders and computes order details.
  - `process_scores`: Gathers and applies product scores based on user feedback.
  - `process_final`: Finalizes the purchase based on user input.

- **Utility Functions:**
  - Deep data copying, parsing product details from files, and managing inter-thread communication with condition variables.

### Data Structures

The application defines various data structures to hold information regarding products, users, orders, and shared memory:

- `Product`: Holds product-related data.
- `Store`: Contains categories and product lists.
- `UserOrder`: Holds order details.
- `GlobalSharedMemory` and `PrivateSharedMemory`: Structures for managing shared data across threads and processes.

### Error Handling

The application includes basic error handling strategies. Processes exit on serious errors, and warnings are printed when issues arise in file operations or dynamic memory allocation.

## Dependencies

- C Compiler (gcc recommended)
- POSIX-compliant Operating System (Linux/Unix-like)
