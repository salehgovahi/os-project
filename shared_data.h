
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
