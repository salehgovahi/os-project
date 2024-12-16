#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>

#define MAX_PRODUCTS 1000
#define MAX_CATEGORIES 8
#define MAX_STORES 3
#define MAX_PATH_LENGTH 1024
#define MAX_NAME_LENGTH 256
#define MAX_ORDER_ITEMS 256
#define MAX_THRESHOLD 1000000.0f
#define MAX_USERS 1000

typedef struct
{
    char name[MAX_NAME_LENGTH];
    float price;
    int entity;
    char last_modified[50];
    float score;
    int rating_count;
} Product;

typedef struct
{
    char category_name[MAX_NAME_LENGTH];
    Product products[MAX_PRODUCTS];
    int product_count;
} Category;

typedef struct
{
    char store_name[MAX_NAME_LENGTH];
    Category categories[MAX_CATEGORIES];
    int category_count;
} Store;

typedef struct
{
    char product_name[MAX_NAME_LENGTH];
    int quantity;
} OrderItem;

typedef struct
{
    char username[MAX_NAME_LENGTH];
    OrderItem order_list[MAX_ORDER_ITEMS];
    int order_count;
    float price_threshold;
} UserOrder;

typedef struct
{
    Product *product;
    const char *store_name;
    const char *category_name;
    char *log_file_path;
    float price_threshold;
    int quantity;
    pthread_t thread_id;
    long int process_id;
    int store_index;
    const UserOrder *user_order;
} ProductContext;

typedef struct
{
    char name[MAX_NAME_LENGTH];
    int entity;
    int wanted_number;
    float score;
    float price;
} SuggestedProduct;

typedef struct
{
    char store_name[MAX_NAME_LENGTH];
    SuggestedProduct products[MAX_PRODUCTS];
    int products_count;
} ShoppingList;

typedef struct
{
    ShoppingList shopping_lists[MAX_STORES];
} SuggestedShoppingLists;

typedef struct
{
    char username[MAX_NAME_LENGTH];
    char store_name[MAX_NAME_LENGTH];
    int has_subscription;
} UserSubscription;

typedef struct
{
    pthread_mutex_t mutex;
    Store stores[MAX_STORES];
    UserSubscription user_subscriptions[MAX_USERS];
    int subscription_count;
} GlobalSharedMemory;
GlobalSharedMemory *global_shared_memory;

typedef struct
{
    SuggestedShoppingLists lists;
    pthread_mutex_t mutex;
    UserOrder user_order;
    char wanted_list_store_name[MAX_NAME_LENGTH];
} PrivateSharedMemory;
PrivateSharedMemory *private_shared_memory;

int parse_product(FILE *file, Product *product)
{
    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        if (sscanf(line, "Name: %[^\n]", product->name) == 1)
            continue;
        if (sscanf(line, "Price: %f", &product->price) == 1)
            continue;
        if (sscanf(line, "Score: %f", &product->score) == 1)
            continue;
        if (sscanf(line, "Entity: %d", &product->entity) == 1)
            continue;
        if (sscanf(line, "Rating Count: %d", &product->rating_count) == 1)
            continue;
        if (sscanf(line, "Last Modified: %[^\n]", product->last_modified) == 1)
            continue;
    }

    if (product->rating_count == 0)
    {
        product->rating_count = 1;
    }

    return 0;
}

int load_dataset(Store *stores, const char *base_path)
{
    DIR *base_dir = opendir(base_path);
    if (!base_dir)
    {
        perror("Failed to open dataset directory");
        return -1;
    }

    struct dirent *store_entry;
    int store_count = 0;

    while ((store_entry = readdir(base_dir)) != NULL)
    {
        if (store_entry->d_name[0] == '.')
            continue;

        char store_path[MAX_PATH_LENGTH];
        snprintf(store_path, sizeof(store_path), "%s/%s", base_path, store_entry->d_name);

        struct stat store_stat;
        if (stat(store_path, &store_stat) != 0)
        {
            perror("Failed to get store stat");
            continue;
        }

        if (S_ISDIR(store_stat.st_mode))
        {
            if (store_count >= MAX_STORES)
            {
                fprintf(stderr, "Warning: Maximum store limit reached. Some stores may not be loaded.\n");
                break;
            }

            snprintf(stores[store_count].store_name, sizeof(stores[store_count].store_name), "%s", store_entry->d_name);

            DIR *store_dir = opendir(store_path);
            if (!store_dir)
            {
                perror("Failed to open store directory");
                continue;
            }

            stores[store_count].category_count = 0;

            struct dirent *category_entry;
            while ((category_entry = readdir(store_dir)) != NULL)
            {
                if (category_entry->d_name[0] == '.')
                    continue;

                char category_path[MAX_PATH_LENGTH];
                snprintf(category_path, sizeof(category_path), "%s/%s", store_path, category_entry->d_name);

                struct stat category_stat;
                if (stat(category_path, &category_stat) != 0)
                {
                    perror("Failed to get category stat");
                    continue;
                }

                if (S_ISDIR(category_stat.st_mode))
                {
                    if (stores[store_count].category_count >= MAX_CATEGORIES)
                    {
                        fprintf(stderr, "Warning: Maximum category limit reached in store %s.\n", stores[store_count].store_name);
                        break;
                    }

                    snprintf(stores[store_count].categories[stores[store_count].category_count].category_name,
                             sizeof(stores[store_count].categories[stores[store_count].category_count].category_name),
                             "%s", category_entry->d_name);

                    DIR *category_dir = opendir(category_path);
                    if (!category_dir)
                    {
                        perror("Failed to open category directory");
                        continue;
                    }

                    stores[store_count].categories[stores[store_count].category_count].product_count = 0;

                    struct dirent *product_entry;
                    while ((product_entry = readdir(category_dir)) != NULL)
                    {
                        if (product_entry->d_name[0] == '.') // نادیده گرفتن "." و ".."
                            continue;

                        char product_file_path[MAX_PATH_LENGTH];
                        snprintf(product_file_path, sizeof(product_file_path), "%s/%s", category_path, product_entry->d_name);

                        struct stat product_stat;
                        if (stat(product_file_path, &product_stat) != 0)
                        {
                            perror("Failed to get product stat");
                            continue;
                        }

                        if (S_ISREG(product_stat.st_mode) && strstr(product_entry->d_name, ".txt") != NULL) // بررسی فایل‌های متنی
                        {
                            FILE *file = fopen(product_file_path, "r");
                            if (!file)
                            {
                                perror("Failed to open product file");
                                continue;
                            }

                            Product product;
                            if (parse_product(file, &product) == 0)
                            {
                                if (stores[store_count].categories[stores[store_count].category_count].product_count < MAX_PRODUCTS)
                                {
                                    stores[store_count].categories[stores[store_count].category_count].products[stores[store_count].categories[stores[store_count].category_count].product_count++] = product;
                                }
                                else
                                {
                                    fprintf(stderr, "Warning: Maximum product limit reached in category %s of store %s.\n",
                                            stores[store_count].categories[stores[store_count].category_count].category_name,
                                            stores[store_count].store_name);
                                }
                            }
                            fclose(file);
                        }
                    }

                    closedir(category_dir);
                    stores[store_count].category_count++;
                }
            }

            closedir(store_dir);
            store_count++;
        }
    }

    closedir(base_dir);
    return store_count;
}
void *process_product(void *arg)
{
    ProductContext *context = (ProductContext *)arg;
    Product *product = context->product;
    const char *store_name = context->store_name;
    const UserOrder *user_order = context->user_order;

    FILE *log_file = fopen(context->log_file_path, "a");
    if (!log_file)
    {
        perror("Failed to open log file in thread");
        free(context->log_file_path);
        free(context);
        return NULL;
    }

    float total_price = product->price * context->quantity;

    fprintf(log_file, "Thread ID: %lu, PID: %ld, Store: %s, Category: %s, Product: %s, Entity: %d, Total Price: %.2f\n",
            (unsigned long)pthread_self(), (long)context->process_id, store_name, context->category_name, product->name, context->quantity, total_price);

    for (int i = 0; i < user_order->order_count; i++)
    {
        if (strcasecmp(product->name, user_order->order_list[i].product_name) == 0)
        {
            if (user_order->order_list[i].quantity > product->entity)
            {
                fprintf(log_file, "Insufficient quantity for %s. Requested: %d, Available: %d. Skipping this product.\n",
                        product->name, user_order->order_list[i].quantity, product->entity);
                break;
            }

            pthread_mutex_lock(&private_shared_memory->mutex);

            int product_count = private_shared_memory->lists.shopping_lists[context->store_index].products_count;
            int product_exists = 0;

            if (product_count == 0)
            {
                strcpy(private_shared_memory->lists.shopping_lists[context->store_index].store_name, store_name);
            }

            for (int j = 0; j < product_count; j++)
            {
                if (strcasecmp(private_shared_memory->lists.shopping_lists[context->store_index].products[j].name, product->name) == 0)
                {
                    product_exists = 1;
                    break;
                }
            }

            if (!product_exists && product_count < MAX_PRODUCTS)
            {
                SuggestedProduct suggested_product;
                strcpy(suggested_product.name, product->name);
                suggested_product.wanted_number = user_order->order_list[i].quantity;
                suggested_product.entity = product->entity;
                suggested_product.score = product->score;
                suggested_product.price = product->price;

                private_shared_memory->lists.shopping_lists[context->store_index].products[product_count] = suggested_product;
                private_shared_memory->lists.shopping_lists[context->store_index].products_count++;
            }

            pthread_mutex_unlock(&private_shared_memory->mutex);
            break;
        }
    }

    fclose(log_file);
    free(context->log_file_path);
    free(context);
    return NULL;
}

pthread_mutex_t order_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t order_cond = PTHREAD_COND_INITIALIZER;
int order_thread_should_run = 0;

void wake_order_thread()
{
    pthread_mutex_lock(&order_mutex);
    order_thread_should_run = 1;
    pthread_cond_signal(&order_cond);
    pthread_mutex_unlock(&order_mutex);
}

void *process_orders(void *args)
{
    pthread_mutex_lock(&order_mutex);
    while (!order_thread_should_run)
    {
        pthread_cond_wait(&order_cond, &order_mutex);
    }
    pthread_mutex_unlock(&order_mutex);

    for (int store_index = 0; store_index < MAX_STORES; store_index++)
    {
        int product_count = private_shared_memory->lists.shopping_lists[store_index].products_count;
        float total_list_value = 0.0f;

        if (product_count > 0)
        {
            printf("Calculating values for Store: %s\n", private_shared_memory->lists.shopping_lists[store_index].store_name);

            for (int product_index = 0; product_index < product_count; product_index++)
            {
                SuggestedProduct product = private_shared_memory->lists.shopping_lists[store_index].products[product_index];
                float product_value = product.price * product.score;
                total_list_value += product_value;

                printf("Product: %s, Price: %.2f, Score: %.2f, Value: %.2f\n",
                       product.name, product.price, product.score, product_value);
            }

            printf("Total value for Store %s: %.2f\n\n", private_shared_memory->lists.shopping_lists[store_index].store_name, total_list_value);
        }
    }

    return NULL;
}

pthread_mutex_t score_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t score_cond = PTHREAD_COND_INITIALIZER;
int score_thread_should_run = 0;

void wake_score_thread()
{
    pthread_mutex_lock(&score_mutex);
    score_thread_should_run = 1;
    pthread_cond_signal(&score_cond);
    pthread_mutex_unlock(&score_mutex);
}
void reset_score_thread()
{
    pthread_mutex_lock(&score_mutex);
    score_thread_should_run = 0; // بازنشانی شرط
    pthread_mutex_unlock(&score_mutex);
}
void *process_scores(void *arg)
{
    pthread_mutex_lock(&score_mutex);
    while (!score_thread_should_run)
    {
        pthread_cond_wait(&score_cond, &score_mutex);
    }
    pthread_mutex_unlock(&score_mutex);

    pthread_mutex_lock(&private_shared_memory->mutex);
    pthread_mutex_lock(&global_shared_memory->mutex);
    const char *wanted_store_name = private_shared_memory->wanted_list_store_name;

    for (int store_index = 0; store_index < MAX_STORES; store_index++)
    {
        if (strcasecmp(private_shared_memory->lists.shopping_lists[store_index].store_name, wanted_store_name) == 0)
        {
            int product_count = private_shared_memory->lists.shopping_lists[store_index].products_count;

            printf("Rating for products in Store: %s\n", private_shared_memory->lists.shopping_lists[store_index].store_name);
            for (int product_index = 0; product_index < product_count; product_index++)
            {
                SuggestedProduct *suggested_product = &private_shared_memory->lists.shopping_lists[store_index].products[product_index];

                float rating;
                printf("Rate the product '%s' (Price: %.2f, Current score: %.2f): ", suggested_product->name, suggested_product->price, suggested_product->score);
                scanf("%f", &rating);
                bool ok = true;
                for (int cat_index = 0; ok && cat_index < global_shared_memory->stores[store_index].category_count; cat_index++)
                {
                    for (int prod_index = 0; ok && prod_index < global_shared_memory->stores[store_index].categories[cat_index].product_count; prod_index++)
                    {
                        Product *store_product = &global_shared_memory->stores[store_index].categories[cat_index].products[prod_index];
                        if (strcasecmp(store_product->name, suggested_product->name) == 0)
                        {
                            store_product->score = ((store_product->score * store_product->rating_count) + rating) / (store_product->rating_count + 1);
                            store_product->rating_count++;

                            time_t now = time(NULL);
                            strftime(store_product->last_modified, sizeof(store_product->last_modified), "%Y-%m-%d %H:%M:%S", localtime(&now));

                            printf("Updated score for product '%s' in store '%s' to %.2f\n", store_product->name, wanted_store_name, store_product->score);
                            printf("Last modified date for product '%s': %s\n", store_product->name, store_product->last_modified);
                            ok = false;
                            break;
                        }
                    }
                }
            }
        }
    }
    pthread_mutex_unlock(&global_shared_memory->mutex);
    pthread_mutex_unlock(&private_shared_memory->mutex);
    return NULL;
}

pthread_mutex_t final_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t final_cond = PTHREAD_COND_INITIALIZER;
int final_thread_should_run = 0;

void wake_final_thread()
{
    pthread_mutex_lock(&final_mutex);
    final_thread_should_run = 1;
    pthread_cond_signal(&final_cond);
    pthread_mutex_unlock(&final_mutex);
}

void *process_final(void *arg)
{
    pthread_mutex_lock(&final_mutex);
    while (!final_thread_should_run)
    {
        pthread_cond_wait(&final_cond, &final_mutex);
    }
    pthread_mutex_unlock(&final_mutex);

    pthread_mutex_lock(&global_shared_memory->mutex);
    pthread_mutex_lock(&private_shared_memory->mutex);
    const char *wanted_store_name = private_shared_memory->wanted_list_store_name;
    int price_threshold = private_shared_memory->user_order.price_threshold;

    for (int store_index = 0; store_index < MAX_STORES; store_index++)
    {
        if (strcasecmp(private_shared_memory->lists.shopping_lists[store_index].store_name, wanted_store_name) == 0)
        {
            int product_count = private_shared_memory->lists.shopping_lists[store_index].products_count;
            float total_list_value = 0.0f;

            if (product_count > 0)
            {
                printf("Calculating values for Store: %s\n", private_shared_memory->lists.shopping_lists[store_index].store_name);

                for (int product_index = 0; product_index < product_count; product_index++)
                {
                    SuggestedProduct product = private_shared_memory->lists.shopping_lists[store_index].products[product_index];
                    float product_value = product.price * product.wanted_number;

                    int discount_applied = 0;
                    for (int user_index = 0; user_index < global_shared_memory->subscription_count; user_index++)
                    {
                        if (strcasecmp(private_shared_memory->user_order.username, global_shared_memory->user_subscriptions[user_index].username) == 0 &&
                            strcasecmp(global_shared_memory->user_subscriptions[user_index].store_name, wanted_store_name) == 0)
                        {
                            if (global_shared_memory->user_subscriptions[user_index].has_subscription)
                            {
                                product_value *= 0.90;
                                discount_applied = 1;
                                printf("Applying 10%% discount for returning customer on %s: %.2f reduced to %.2f\n", product.name, product.price, product_value);
                                break;
                            }
                        }
                    }

                    total_list_value += product_value;

                    for (int category_index = 0; category_index < global_shared_memory->stores[store_index].category_count; category_index++)
                    {
                        for (int prod_index = 0; prod_index < global_shared_memory->stores[store_index].categories[category_index].product_count; prod_index++)
                        {
                            Product *store_product = &global_shared_memory->stores[store_index].categories[category_index].products[prod_index];
                            if (strcasecmp(store_product->name, product.name) == 0)
                            {
                                int new_entity = store_product->entity - product.wanted_number;
                                if (new_entity < 0)
                                {
                                    printf("Warning: Trying to reduce quantity for %s below zero. Current entity: %d, Requested: %d.\n",
                                           store_product->name, store_product->entity, product.wanted_number);
                                    new_entity = 0;
                                }
                                store_product->entity = new_entity;

                                time_t now = time(NULL);
                                strftime(store_product->last_modified, sizeof(store_product->last_modified), "%Y-%m-%d %H:%M:%S", localtime(&now));

                                printf("Updated product %s in store %s. New entity: %d, Last modified: %s\n",
                                       store_product->name, wanted_store_name, store_product->entity, store_product->last_modified);
                                break;
                            }
                        }
                    }
                }

                printf("Total value for Store %s after possible discount: %.2f\n\n", private_shared_memory->lists.shopping_lists[store_index].store_name, total_list_value);
            }

            int user_subscription_index = -1;
            for (int user_index = 0; user_index < global_shared_memory->subscription_count; user_index++)
            {
                if (strcasecmp(private_shared_memory->user_order.username, global_shared_memory->user_subscriptions[user_index].username) == 0 &&
                    strcasecmp(global_shared_memory->user_subscriptions[user_index].store_name, wanted_store_name) == 0)
                {
                    user_subscription_index = user_index;
                    break;
                }
            }

            if (user_subscription_index == -1)
            {
                if (global_shared_memory->subscription_count < MAX_USERS)
                {
                    strncpy(global_shared_memory->user_subscriptions[global_shared_memory->subscription_count].username, private_shared_memory->user_order.username, sizeof(global_shared_memory->user_subscriptions[global_shared_memory->subscription_count].username));
                    strncpy(global_shared_memory->user_subscriptions[global_shared_memory->subscription_count].store_name, wanted_store_name, sizeof(global_shared_memory->user_subscriptions[global_shared_memory->subscription_count].store_name));
                    global_shared_memory->user_subscriptions[global_shared_memory->subscription_count].has_subscription = 1;
                    global_shared_memory->subscription_count++;
                }
            }
        }
    }
    pthread_mutex_unlock(&global_shared_memory->mutex);
    pthread_mutex_unlock(&private_shared_memory->mutex);

    return NULL;
}

void deep_copy_stores(Store *destination, const Store *source, int store_count)
{
    for (int i = 0; i < store_count; i++)
    {
        strcpy(destination[i].store_name, source[i].store_name);
        destination[i].category_count = source[i].category_count;

        for (int j = 0; j < source[i].category_count; j++)
        {
            strcpy(destination[i].categories[j].category_name, source[i].categories[j].category_name);
            destination[i].categories[j].product_count = source[i].categories[j].product_count;

            for (int k = 0; k < source[i].categories[j].product_count; k++)
            {
                strcpy(destination[i].categories[j].products[k].name, source[i].categories[j].products[k].name);
                destination[i].categories[j].products[k].price = source[i].categories[j].products[k].price;
                destination[i].categories[j].products[k].entity = source[i].categories[j].products[k].entity;
                destination[i].categories[j].products[k].score = source[i].categories[j].products[k].score;
            }
        }
    }
}

int main()
{
    Store stores[MAX_STORES];
    const char *base_path = "Dataset";
    const char *output_directory = "Output";

    mkdir(output_directory, 0755);

    int store_count = load_dataset(stores, base_path);

    int shm_fd = shm_open("/global_shared_memory", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shm_fd, sizeof(GlobalSharedMemory)) == -1)
    {
        perror("ftruncate");
        exit(1);
    }

    global_shared_memory = mmap(NULL, sizeof(GlobalSharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (global_shared_memory == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&global_shared_memory->mutex, &mutex_attr);

    deep_copy_stores(global_shared_memory->stores, stores, store_count);

    // printf("Suggestions for Store: %s\n", global_shared_memory->stores[0].store_name);
    // printf("Suggestions for Store: %s\n", stores[0].store_name);
    while (1)
    {
        printf("Enter 'exit' to quit or press ENTER to continue for a new user.\n");

        char input[10];
        fgets(input, sizeof(input), stdin);

        if (strncmp(input, "exit", 4) == 0)
        {
            printf("Exiting the program.\n");
            break;
        }

        pid_t user_pid = fork();
        if (user_pid < 0)
        {
            perror("Error forking user process");
            exit(EXIT_FAILURE);
        }

        if (user_pid == 0)
        {
            int shm_fd = shm_open("/private_shared_memory", O_CREAT | O_RDWR, 0666);
            if (shm_fd == -1)
            {
                perror("shm_open");
                exit(1);
            }

            if (ftruncate(shm_fd, sizeof(PrivateSharedMemory)) == -1)
            {
                perror("ftruncate");
                exit(1);
            }

            private_shared_memory = mmap(NULL, sizeof(PrivateSharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
            if (private_shared_memory == MAP_FAILED)
            {
                perror("mmap");
                exit(1);
            }

            pthread_mutexattr_t mutex_attr;
            pthread_mutexattr_init(&mutex_attr);
            pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
            pthread_mutex_init(&private_shared_memory->mutex, &mutex_attr);

            UserOrder user_order;
            printf("Username: ");
            fgets(user_order.username, sizeof(user_order.username), stdin);
            user_order.username[strcspn(user_order.username, "\n")] = 0;

            printf("Enter your order list (product_name quantity), type 'done' when finished:\n");
            user_order.order_count = 0;

            while (user_order.order_count < MAX_ORDER_ITEMS)
            {
                char line[256];
                printf("Product and quantity: ");
                if (fgets(line, sizeof(line), stdin) == NULL || strcmp(line, "done\n") == 0)
                    break;

                char product_name[MAX_NAME_LENGTH];
                int quantity;
                if (sscanf(line, "%s %d", product_name, &quantity) != 2)
                {
                    printf("Invalid input. Use 'product_name quantity'.\n");
                    continue;
                }

                strncpy(user_order.order_list[user_order.order_count].product_name, product_name, MAX_NAME_LENGTH);
                user_order.order_list[user_order.order_count].quantity = quantity;
                printf("Added: '%s' (Quantity: %d)\n", product_name, quantity);
                user_order.order_count++;
            }

            printf("Price threshold (default is %.2f): ", MAX_THRESHOLD);
            char input_buffer[256];
            fgets(input_buffer, sizeof(input_buffer), stdin);

            if (input_buffer[0] == '\n')
            {
                user_order.price_threshold = MAX_THRESHOLD;
                printf("No input provided. Setting price threshold to default value: %.2f\n", MAX_THRESHOLD);
            }
            else
            {
                int threshold_status = sscanf(input_buffer, "%f", &user_order.price_threshold);

                if (threshold_status != 1 || user_order.price_threshold <= 0)
                {
                    user_order.price_threshold = MAX_THRESHOLD;
                }
            }

            memset(&private_shared_memory->user_order, 0, sizeof(UserOrder));
            private_shared_memory->user_order = user_order;
            memset(&private_shared_memory->lists, 0, sizeof(SuggestedShoppingLists));
            //          private_shared_memory->lists.shopping_lists[0].products_count = store_count;

            sleep(2);

            printf("User Process (PID: %d)\n", getpid());

            pthread_t order_thread_id;
            if (pthread_create(&order_thread_id, NULL, process_orders, NULL) == 0)
            {
                printf("PID %d created thread for Orders (TID: %lu)\n", getpid(), order_thread_id);
            }
            else
            {
                perror("Failed to create Order thread");
            }

            pthread_t score_thread_id;
            if (pthread_create(&score_thread_id, NULL, process_scores, NULL) == 0)
            {
                printf("PID %d created thread for Scores (TID: %lu)\n", getpid(), score_thread_id);
            }
            else
            {
                perror("Failed to create Score thread");
            }

            pthread_t final_thread_id;
            if (pthread_create(&final_thread_id, NULL, process_final, NULL) == 0)
            {
                printf("PID %d created thread for Final (TID: %lu)\n", getpid(), final_thread_id);
            }
            else
            {
                perror("Failed to create Final thread");
            }
            for (int m = 0; m < private_shared_memory->user_order.order_count; m++)
            {
                for (int i = 0; i < MAX_STORES; i++)
                {
                    pid_t store_pid = fork();
                    if (store_pid < 0)
                    {
                        perror("Error forking store process");
                        exit(EXIT_FAILURE);
                    }

                    if (store_pid == 0)
                    {
                        printf("PID %d create child for Store%d PID:%d\n", getppid(), i + 1, getpid());
                        printf("Store Process (PID: %d) for Store: %s\n", getpid(), global_shared_memory->stores[i].store_name);

                        for (int j = 0; j < global_shared_memory->stores[i].category_count; j++)
                        {
                            pid_t category_pid = fork();
                            if (category_pid < 0)
                            {
                                perror("Error forking category process");
                                exit(EXIT_FAILURE);
                            }

                            if (category_pid == 0)
                            {
                                printf("Category Process (PID: %d) for Store: %s, Category: %s\n", getpid(), global_shared_memory->stores[i].store_name, global_shared_memory->stores[i].categories[j].category_name);

                                char log_file_path[MAX_PATH_LENGTH];
                                snprintf(log_file_path, sizeof(log_file_path), "%s/log_%s", output_directory, user_order.username);

                                pthread_t threads[MAX_PRODUCTS];
                                int thread_count = 0;

                                for (int k = 0; k < global_shared_memory->stores[i].categories[j].product_count; k++)
                                {
                                    ProductContext *context = malloc(sizeof(ProductContext));
                                    context->product = &global_shared_memory->stores[i].categories[j].products[k];
                                    context->store_name = global_shared_memory->stores[i].store_name;
                                    context->category_name = global_shared_memory->stores[i].categories[j].category_name;
                                    context->log_file_path = strdup(log_file_path);
                                    context->price_threshold = user_order.price_threshold;
                                    context->quantity = 1;
                                    context->process_id = getpid();
                                    context->store_index = i;
                                    context->user_order = &private_shared_memory->user_order;

                                    pthread_create(&threads[thread_count], NULL, process_product, context);
                                    // {
                                    //     printf("PID %d created thread for product '%s' in category '%s' (TID: %lu)\n",
                                    //            getpid(),
                                    //            context->product->name,
                                    //            context->category_name,
                                    //            threads[thread_count]);
                                    // }
                                    // else
                                    // {
                                    //     perror("Failed to create thread");
                                    // }
                                    thread_count++;
                                }

                                for (int k = 0; k < thread_count; k++)
                                {
                                    pthread_join(threads[k], NULL);
                                }

                                exit(0);
                            }
                        }

                        pid_t child_pid;
                        while ((child_pid = waitpid(-1, NULL, 0)) > 0)
                        {
                            // printf("Child process PID %d finished.\n", child_pid);
                        }
                        exit(0);
                    }
                }

                while (wait(NULL) > 0)
                    ;
            }

            sleep(3);

            printf("\nSuggested Shopping Lists:\n");
            for (int j = 0; j < store_count; j++)
            {
                if (private_shared_memory->lists.shopping_lists[j].products_count > 0)
                {
                    printf("Suggestions for Store: %s\n", private_shared_memory->lists.shopping_lists[j].store_name);
                    bool ok = true;
                    float totalPrice = 0;

                    for (int k = 0; k < private_shared_memory->lists.shopping_lists[j].products_count; k++)
                    {

                        totalPrice += private_shared_memory->lists.shopping_lists[j].products[k].price *
                                      private_shared_memory->lists.shopping_lists[j].products[k].wanted_number;
                        if (private_shared_memory->lists.shopping_lists[j].products[k].wanted_number >
                            private_shared_memory->lists.shopping_lists[j].products[k].entity)
                        {
                            ok = false;
                        }
                        if (totalPrice > private_shared_memory->user_order.price_threshold)
                            ok = false;
                    }
                    for (int k = 0; ok && k < private_shared_memory->lists.shopping_lists[j].products_count; k++)
                    {

                        printf("Product: %s, Entity: %d, Price: %.2f, Score: %.2f\n",
                               private_shared_memory->lists.shopping_lists[j].products[k].name,
                               private_shared_memory->lists.shopping_lists[j].products[k].entity,
                               private_shared_memory->lists.shopping_lists[j].products[k].price,
                               private_shared_memory->lists.shopping_lists[j].products[k].score);
                    }
                    if (!ok)
                    {
                        printf("There is not enough stock for a requested item or your amount of money is insufficient\n");
                    }
                    printf("\n");
                }
            }
            while (wait(NULL) > 0)
                ;

            wake_order_thread();

            sleep(5);

            char wanted_list_store_name[MAX_NAME_LENGTH];
            printf("Enter the name of the store to finalize your purchase: ");
            scanf(" %[^\n]", wanted_list_store_name);

            pthread_mutex_lock(&private_shared_memory->mutex);
            strncpy(private_shared_memory->wanted_list_store_name, wanted_list_store_name, sizeof(private_shared_memory->wanted_list_store_name));
            pthread_mutex_unlock(&private_shared_memory->mutex);

            if (sscanf(wanted_list_store_name, " %[^\n]", private_shared_memory->wanted_list_store_name) != 1)
            {
                fprintf(stderr, "Error capturing store name.\n");
            }

            pthread_mutex_unlock(&private_shared_memory->mutex);

            sleep(5);

            wake_final_thread();
            if (pthread_join(final_thread_id, NULL) != 0)
            {
                perror("Failed to join final_thread_id");
            }

            sleep(5);
            wake_score_thread();
                 if (pthread_join(score_thread_id, NULL) != 0)
            {
                perror("Failed to join score_thread_id");
            }

            sleep(2);

             while (wait(NULL) > 0)
                ;
            printf("User %s processing completed.\n", user_order.username);
            pthread_mutex_destroy(&private_shared_memory->mutex);
            munmap(private_shared_memory, sizeof(PrivateSharedMemory));
            shm_unlink("/private_shared_memory");
            exit(0);
        }
        while (wait(NULL) > 0)
            ;
        printf("User process completed. Ready for the next user.\n");
    }
    pthread_mutex_destroy(&global_shared_memory->mutex);
    munmap(global_shared_memory, sizeof(GlobalSharedMemory));
    shm_unlink("/global_shared_memory");

    return 0;
}