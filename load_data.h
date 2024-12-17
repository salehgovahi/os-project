
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