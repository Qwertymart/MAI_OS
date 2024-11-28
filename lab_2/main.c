#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#define MAX_POINTS 100000
#define MAX_CLUSTERS 100

typedef struct {
    double x, y;
} Point;

typedef struct {
    int start, end;
    Point* points;
    Point* centroids;
    int* point_cluster;
    int num_clusters;
} Thread_data;


pthread_mutex_t mutexes[MAX_CLUSTERS];
double cluster_sums_x[MAX_CLUSTERS];
double cluster_sums_y[MAX_CLUSTERS];
int cluster_counts[MAX_CLUSTERS];

void write_message(const char* message) {
    write(STDOUT_FILENO, message, strlen(message));
}

void* assign_clusters(void* arg) {
    Thread_data* data = (Thread_data*)arg;

    for (int i = data->start; i < data->end; i++) {
        double min_dist = INFINITY;
        int closest_cluster = -1;

        for (int j = 0; j < data->num_clusters; j++)
        {
            double dist = sqrt(pow(data->points[i].x - data->centroids[j].x, 2) +
                               pow(data->points[i].y - data->centroids[j].y, 2));
            if (dist < min_dist) {
                min_dist = dist;
                closest_cluster = j;
            }
        }

        data->point_cluster[i] = closest_cluster;

        pthread_mutex_lock(&mutexes[closest_cluster]);
        cluster_sums_x[closest_cluster] += data->points[i].x;
        cluster_sums_y[closest_cluster] += data->points[i].y;
        cluster_counts[closest_cluster]++;
        pthread_mutex_unlock(&mutexes[closest_cluster]);
    }

    return NULL;
}

void update_centroids(Point* centroids, int num_clusters) {
    for (int i = 0; i < num_clusters; i++) {
        if (cluster_counts[i] > 0) {
            centroids[i].x = cluster_sums_x[i] / cluster_counts[i];
            centroids[i].y = cluster_sums_y[i] / cluster_counts[i];
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        write_message("Usage: ./program <num_clusters> <num_threads>\n");
        return 1;
    }

    int num_clusters = atoi(argv[1]);
    int num_threads = atoi(argv[2]);

    if (num_clusters > MAX_CLUSTERS || num_threads < 1) {
        write_message("Invalid number of clusters or threads.\n");
        return 1;
    }

    int num_points = 10000;
    Point points[num_points];
    Point centroids[num_clusters];
    Point prev_centroids[num_clusters];
    int point_cluster[num_points];

    pthread_t threads[num_threads];
    Thread_data thread_data[num_threads];

    FILE* file = fopen("test", "r");

    for (int i = 0; i < num_points; i++) {
        if (fscanf(file, "%lf %lf", &points[i].x, &points[i].y) != 2) {
            fprintf(stderr, "Error reading point %d from file\n", i);
            fclose(file);
            return 1;
        }
    }

    fclose(file);

    for (int i = 0; i < num_clusters; i++) {
        pthread_mutex_init(&mutexes[i], NULL);
    }

    clock_t start_time = clock();

    for (int i = 0; i < num_clusters; i++) {
        centroids[i] = points[i];
    }

    int flag = 0, iterations = 0;
    while (!flag) {
        for (int i = 0; i < num_clusters; i++) {
            prev_centroids[i] = centroids[i];
        }

        memset(cluster_sums_x, 0, sizeof(cluster_sums_x));
        memset(cluster_sums_y, 0, sizeof(cluster_sums_y));
        memset(cluster_counts, 0, sizeof(cluster_counts));


        int chunk_size = (num_points + num_threads - 1) / num_threads;
        for (int i = 0; i < num_threads; i++) {
            thread_data[i].start = i * chunk_size;
            thread_data[i].end = (i + 1) * chunk_size > num_points ? num_points : (i + 1) * chunk_size;
            thread_data[i].points = points;
            thread_data[i].centroids = centroids;
            thread_data[i].point_cluster = point_cluster;
            thread_data[i].num_clusters = num_clusters;

            pthread_create(&threads[i], NULL, assign_clusters, &thread_data[i]);
        }

        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }

        update_centroids(centroids, num_clusters);

        flag = 1;
        for (int i = 0; i < num_clusters; i++) {
            if (fabs(centroids[i].x - prev_centroids[i].x) > 1e-4 ||
                fabs(centroids[i].y - prev_centroids[i].y) > 1e-4) {
                flag = 0;
                break;
            }
        }

        iterations++;

        if (iterations > 1000) {
            write_message("Iteration limit reached. Stopping.\n");
            break;
        }
    }

    printf("Clustering completed in %d iterations.\n", iterations);
    for (int j = 0; j < num_clusters; ++j)
    {
        printf("Cluster %d: %lf %lf\n", j + 1, centroids[j].x, centroids[j].y);
    }

    printf("Execution time: %.6f seconds\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);

    for (int i = 0; i < num_clusters; i++) {
        pthread_mutex_destroy(&mutexes[i]);
    }


    return 0;
}






