#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

// *** Structures and Constants (from your provided code) ***
#define MAX_PRODUCTS 1000
#define MAX_CATEGORIES 8
#define MAX_STORES 3
#define MAX_PATH_LENGTH 1024
#define MAX_NAME_LENGTH 256
#define MAX_ORDER_ITEMS 256
#define MAX_THRESHOLD 1000000.0f
#define MAX_USERS 1000

typedef struct {
    char name[MAX_NAME_LENGTH];
    float price;
    int entity;
    char last_modified[50];
    float score;
    int rating_count;
} Product;

typedef struct {
    char category_name[MAX_NAME_LENGTH];
    Product products[MAX_PRODUCTS];
    int product_count;
} Category;

typedef struct {
    char store_name[MAX_NAME_LENGTH];
    Category categories[MAX_CATEGORIES];
    int category_count;
} Store;

typedef struct {
    char product_name[MAX_NAME_LENGTH];
    int quantity;
} OrderItem;

typedef struct {
    char username[MAX_NAME_LENGTH];
    OrderItem order_list[MAX_ORDER_ITEMS];
    int order_count;
    float price_threshold;
} UserOrder;

typedef struct {
    Store stores[MAX_STORES];
    int store_count;
} AppData;

AppData app_data;

// *** GTK Widgets ***
GtkWidget *entry_username;
GtkWidget *entry_product_name;
GtkWidget *entry_quantity;
GtkWidget *order_list_view;
GtkWidget *status_label;

// *** Data for Orders ***
UserOrder user_order;

// *** Helper Functions ***

void add_order_to_list_view(const char *product_name, int quantity) {
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(order_list_view)));
    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, product_name, 1, quantity, -1);
}

void reset_order_list_view() {
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(order_list_view)));
    gtk_list_store_clear(store);
}

void update_status(const char *message) {
    gtk_label_set_text(GTK_LABEL(status_label), message);
}

// *** GTK Callbacks ***

void on_add_order_clicked(GtkWidget *widget, gpointer data) {
    const char *product_name = gtk_entry_get_text(GTK_ENTRY(entry_product_name));
    const char *quantity_str = gtk_entry_get_text(GTK_ENTRY(entry_quantity));
    int quantity = atoi(quantity_str);

    if (strlen(product_name) == 0 || quantity <= 0) {
        update_status("Please enter a valid product name and quantity.");
        return;
    }

    if (user_order.order_count >= MAX_ORDER_ITEMS) {
        update_status("Maximum order items reached.");
        return;
    }

    strncpy(user_order.order_list[user_order.order_count].product_name, product_name, MAX_NAME_LENGTH);
    user_order.order_list[user_order.order_count].quantity = quantity;
    user_order.order_count++;

    add_order_to_list_view(product_name, quantity);
    update_status("Product added to order.");
}

void on_submit_order_clicked(GtkWidget *widget, gpointer data) {
    const char *username = gtk_entry_get_text(GTK_ENTRY(entry_username));

    if (strlen(username) == 0) {
        update_status("Please enter a valid username.");
        return;
    }

    strncpy(user_order.username, username, MAX_NAME_LENGTH);
    user_order.price_threshold = MAX_THRESHOLD;

    update_status("Order submitted. Processing...");
    sleep(1); // Simulate processing
    update_status("Order processed successfully.");
}

void on_clear_order_clicked(GtkWidget *widget, gpointer data) {
    user_order.order_count = 0;
    reset_order_list_view();
    update_status("Order list cleared.");
}

// *** GTK Initialization ***

void setup_order_list_view(GtkWidget *view) {
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column;

    GtkListStore *store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);

    gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));
    g_object_unref(store);

    column = gtk_tree_view_column_new_with_attributes("Product Name", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

    column = gtk_tree_view_column_new_with_attributes("Quantity", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *grid;

    GtkWidget *label_username, *label_product_name, *label_quantity;
    GtkWidget *button_add_order, *button_submit_order, *button_clear_order;

    gtk_init(&argc, &argv);

    // Main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Shopping System");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Grid layout
    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);

    // Username entry
    label_username = gtk_label_new("Username:");
    gtk_grid_attach(GTK_GRID(grid), label_username, 0, 0, 1, 1);

    entry_username = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry_username, 1, 0, 2, 1);

    // Product entry
    label_product_name = gtk_label_new("Product Name:");
    gtk_grid_attach(GTK_GRID(grid), label_product_name, 0, 1, 1, 1);

    entry_product_name = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry_product_name, 1, 1, 1, 1);

    // Quantity entry
    label_quantity = gtk_label_new("Quantity:");
    gtk_grid_attach(GTK_GRID(grid), label_quantity, 2, 1, 1, 1);

    entry_quantity = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry_quantity, 3, 1, 1, 1);

    // Order list view
    order_list_view = gtk_tree_view_new();
    setup_order_list_view(order_list_view);
    GtkWidget *scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll_window), order_list_view);
    gtk_grid_attach(GTK_GRID(grid), scroll_window, 0, 2, 4, 1);

    // Buttons
    button_add_order = gtk_button_new_with_label("Add to Order");
    g_signal_connect(button_add_order, "clicked", G_CALLBACK(on_add_order_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), button_add_order, 0, 3, 1, 1);

    button_submit_order = gtk_button_new_with_label("Submit Order");
    g_signal_connect(button_submit_order, "clicked", G_CALLBACK(on_submit_order_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), button_submit_order, 1, 3, 1, 1);

    button_clear_order = gtk_button_new_with_label("Clear Order");
    g_signal_connect(button_clear_order, "clicked", G_CALLBACK(on_clear_order_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), button_clear_order, 2, 3, 1, 1);

    // Status label
    status_label = gtk_label_new("Status: Ready");
    gtk_grid_attach(GTK_GRID(grid), status_label, 0, 4, 4, 1);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
