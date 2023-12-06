#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define AXONS 256
#define NEURONS_PER_CORE 256
#define NUM_CORES 5
#define FIFO_SIZE 256
#define NUM_PACKETS 22
// #define SYNAPSES 32

typedef struct {
    int current_membrane_potential : 9;
    int reset_posi_potential : 9;
    int weights_0 : 9;
    int weights_1: 9;
    int weights_2 : 9;
    int weights_3 : 9;
    int leakage_value : 9;
    int positive_threshold : 9;
    int negative_threshold : 9;
    int reset_mode : 1;
    int dx : 9;
    int dy : 9;
    unsigned int destination_axon : 8;
    int destination_ticks : 4;
} Neuron;
typedef struct {
    int packet_dx : 9;
    int packet_dy : 9;
    unsigned int packet_destination_axon : 8;
    int packet_destination_ticks : 4;
} packet_in;

typedef struct {
    int front, rear, size;
    unsigned capacity;
    int* array;
} Queue;

typedef struct {
    packet_in packets[NUM_PACKETS];
    Neuron neurons[NEURONS_PER_CORE];
    uint8_t synapse_connections[AXONS][NEURONS_PER_CORE];
    Queue destinationAxonQueue;
    uint8_t output_axons[NEURONS_PER_CORE];
} SNNCore;

SNNCore cores[NUM_CORES];

Queue* createQueue(unsigned capacity) {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0; 
    queue->rear = capacity - 1;
    queue->array = (int*)malloc(queue->capacity * sizeof(int));
    return queue;
}

int isFull(Queue* queue) { return (queue->size == queue->capacity); }
int isEmpty(Queue* queue) { return (queue->size == 0); }

void enqueue(Queue* queue, int item) {
    if (isFull(queue)) return;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->array[queue->rear] = item;
    queue->size = queue->size + 1;
}

int dequeue(Queue* queue) {
    if (isEmpty(queue)) return -1;
    int item = queue->array[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size = queue->size - 1;
    return item;
}

int front(Queue* queue) {
    if (isEmpty(queue)) return -1;
    return queue->array[queue->front];
}

void readNeuronData(SNNCore* core, const char* line, int neuronIndex) {
    // printf("Synaptic connection:");
    for (int i = 0; i < AXONS; i++) {
        core->synapse_connections[i][neuronIndex] = line[i] - '0';
        // printf("%d",  core->synapse_connections[i][neuronIndex]);
    }
    // printf("\n");

    int params[9];
    for (int i = 0, j = AXONS; i < 9; i++, j += 9) {
        int value = 0;
        for (int k = 0; k < 9; k++) {
            value = (value << 1) | (line[j + k] - '0');
        }
        params[i] = value;
    }

    Neuron* neuron = &core->neurons[neuronIndex];
    neuron->current_membrane_potential = params[0];
    neuron->reset_posi_potential = params[1];
    neuron->weights_0 = params[2];
    neuron->weights_1 = params[3];
    neuron->weights_2 = params[4];
    neuron->weights_3 = params[5];
    neuron->leakage_value = params[6];
    neuron->positive_threshold = params[7];
    neuron->negative_threshold = params[8];
    int params0;
    for (int i = 0, j = AXONS+81; i < 1; i++, j += 1) {
        int value = 0;
        for (int k = 0; k < 1; k++) {
            value = (value << 1) | (line[j + k] - '0');
        }
        params0 = value;
    }
    neuron->reset_mode = params0;
    int params1[2];
    for (int i = 0, j = AXONS+82; i < 2; i++, j += 9) {
        int value = 0;
        for (int k = 0; k < 9; k++) {
            value = (value << 1) | (line[j + k] - '0');
        }
        params1[i] = value;
    }
    neuron->dx = params1[0];
    neuron->dy = params1[1];
    int params2;
    for (int i = 0, j = AXONS + 100; i < 1; i++, j += 8) {
        int value = 0;
        for (int k = 0; k < 8; k++) {
            value = (value << 1) | (line[j + k] - '0');
        }
        params2 = value;
    }
    neuron->destination_axon = params2; // Sử dụng 8 bit cho destination_axon
    int params3;
    for (int i = 0, j = AXONS + 108; i < 1; i++, j += 4) {
        int value = 0;
        for (int k = 0; k < 4; k++) {
            value = (value << 1) | (line[j + k] - '0');
        }
        params3 = value;
    }
    neuron->destination_ticks = params3;
}

int getNeuronData(SNNCore cores[]) {
    FILE* file = fopen("neuron_data.txt", "r");
    if (file == NULL) {
        printf("Unable to open file.\n");
        return 1;
    }
    char line[AXONS + 116];
    int coreIndex = 0, neuronIndex = 0;
    int neuronCount = 0; // Đếm số neuron đã in

    while (fgets(line, sizeof(line), file)) {
        if (neuronCount >= 256) {
            break; // Đã in đủ 256 neurons, kết thúc việc in
        }

        if (neuronCount >= 0 && neuronCount < NUM_CORES * NEURONS_PER_CORE) {
            // In thông tin neuron
            // printf("Neuron %d\n", neuronCount);
            readNeuronData(&cores[coreIndex], line, neuronIndex);
            neuronIndex++;
            // printf("\n");
            neuronCount++;
        }

        // Điều chỉnh index cho core và neuron
        if (neuronIndex >= NEURONS_PER_CORE) {
            neuronIndex = 0;
            coreIndex++;
            if (coreIndex >= NUM_CORES) {
                break; // Đã duyệt qua tất cả các cores
            }
        }
    }
    fclose(file);
    return 0;
}

int getWeight(Neuron* neuron, int index) {
    switch (index) {
        case 0:
            return neuron->weights_0;
        case 1:
            return neuron->weights_1;
        case 2:
            return neuron->weights_2;
        case 3:
            return neuron->weights_3;
        default:
            return 0; // Giá trị mặc định hoặc xử lý lỗi
    }
}
void processSpikeEvent(SNNCore* core, int axonIndex) {
    FILE *output_file = fopen("parameter_after.txt", "w");

    if (core->synapse_connections[axonIndex]) {
        for (int i = 0; i < NEURONS_PER_CORE; i++) {
            if (core->synapse_connections[axonIndex][i]) {
                Neuron *neuron = &core->neurons[i];
                neuron->current_membrane_potential += getWeight(neuron, i % 4);
                neuron->current_membrane_potential -= neuron->leakage_value;
                fprintf(output_file, "Current Membrane Potential for Neuron %d: %d\n", i, neuron->current_membrane_potential);

                if (neuron->current_membrane_potential >= neuron->positive_threshold) {
                    neuron->current_membrane_potential = neuron->reset_posi_potential;
                    core->output_axons[i] = 1;
                    /*printf("\nNeuron %d fired spike at axon index %d", i, neuron->destination_axon);*/
                    fprintf(output_file, "%d->neuron %d này có bắn spike\n", core->output_axons[i],i);
                    enqueue(&core->destinationAxonQueue, neuron->destination_axon);
                } else if (neuron->current_membrane_potential <= neuron->negative_threshold) {
                    neuron->current_membrane_potential = neuron->reset_posi_potential;
                    core->output_axons[i] = 0;
                    fprintf(output_file, "%d->neuron %d này không bắn spike\n", core->output_axons[i],i);
                }
            }
        }
    }
}
/// Begin
void readPacketData(SNNCore* core, const char* line, int packetIndex) {
    Queue* destinationAxonQueue = createQueue(NUM_PACKETS);
    Queue* lastdestinationAxonQueue = NULL;
    for( int coreIndex = 0; coreIndex <NUM_CORES; coreIndex ++)
    {
        int params[2];
        for (int i = 0, j = 0; i < 9; i++, j += 9) {
            int value = 0;
            for (int k = 0; k < 9; k++) {
                value = (value << 1) | (line[j + k] - '0');
            }
            params[i] = value;
        }

        packet_in* packet = &core->packets[packetIndex];
        packet->packet_dx = params[0];
        packet->packet_dy = params[1];
        int params0;
        for (int i = 0, j = 18; i < 1; i++, j += 8) {
            int value = 0;
            for (int k = 0; k < 8; k++) {
                value = (value << 1) | (line[j + k] - '0');
            }
            params0 = value;
        }
        packet->packet_destination_axon = params0;
        enqueue(&cores[coreIndex].destinationAxonQueue, packet->packet_destination_axon);

        int params1;
        for (int i = 0, j = 26; i < 1; i++, j += 4) {
            int value = 0;
            for (int k = 0; k < 4; k++) {
                value = (value << 1) | (line[j + k] - '0');
            }
            params1 = value;
        }
        packet->packet_destination_ticks = params1;
        lastdestinationAxonQueue = destinationAxonQueue;

        printf("packet_dx: %d\n",packet->packet_dx);
        printf("packet_dy: %d\n",packet->packet_dy);
        printf("packet_Destination Axon: %d\n",packet->packet_destination_axon);
        printf("packet_Destination Ticks: %d\n",packet->packet_destination_ticks);
    }
}

int getPacketData(SNNCore cores[]) {
    FILE* file = fopen("spike_in.txt", "r");
    if (file == NULL) {
        printf("Unable to open file.\n");
        return 1;
    }
    char line[34];
    int coreIndex = 0, packetIndex = 0;
    int packetCount = 0; // Đếm số packet đã in

    while (fgets(line, sizeof(line), file)) {
        if (packetCount >= NUM_PACKETS) {
            break; // Đã in đủ số packet, kết thúc việc in
        }
        if (packetCount >= 0 && packetCount < NUM_PACKETS) {
            // In thông tin neuron
            printf("Packet %d\n", packetCount);
            readPacketData(&cores[coreIndex], line, packetIndex);
            packetIndex++;
            printf("\n");
            packetCount++;
        }
        // Điều chỉnh index cho core và packet
        if (packetIndex >= NEURONS_PER_CORE) {
            packetIndex = 0;
            coreIndex++;
            if (coreIndex >= NUM_CORES) {
                break; // Đã duyệt qua tất cả các cores
            }
        }
    }
    fclose(file);
    return 0;
}

void printQueue(Queue* queue) {
    printf("Queue contents: \n");
    for (int i = queue->front; i <= queue->rear; i++) {
        printf("%d ", queue->array[i]);
    }
    printf("\n");
}
/// End

void initializeCoreQueues() {
    for (int i = 0; i < NUM_CORES; i++) {
        cores[i].destinationAxonQueue = *createQueue(FIFO_SIZE);
    }
}

void saveOutputAxons(SNNCore* cores, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        printf("Unable to open file %s for writing.\n", filename);
        return;
    }
    for(int coreindex = 0; coreindex < NUM_CORES ; coreindex ++)
    {
        for (int axonIndex = 0; axonIndex < NEURONS_PER_CORE; axonIndex++) {
            fprintf(file, "%d", cores[coreindex].output_axons[axonIndex]);
        }
    }
    fprintf(file, "\n");

    fclose(file);
}

int main() {

    initializeCoreQueues();
    if (getNeuronData(cores)) {
        return 1;
    }
    if (getPacketData(cores)) {
        return 1;
    }

    for (int i = 0; i < NUM_CORES; i++) {
        int axonIndex = dequeue(&cores[i].destinationAxonQueue);
        
        printQueue(&cores[i].destinationAxonQueue);

        processSpikeEvent(&cores[i],axonIndex);
        printf("\nOutput spikes for Core %d:", i);
        printf("\n");
        for(int axon = 0; axon < AXONS; axon++) {
            printf("%d", cores[i].output_axons[axon]);
        }
    }

    saveOutputAxons(cores, "output_axons.txt");
    return 0;
}