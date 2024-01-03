#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define AXONS 256
#define NEURONS_PER_CORE 256
#define NUM_CORES 5
#define FIFO_SIZE 1000
#define GROUPS 32

typedef struct {
    int8_t current_membrane_potential;
    int8_t reset_posi_potential;
    int8_t reset_nega_potential;
    int8_t weights[4];
    int8_t leakage_value;
    int8_t positive_threshold;
    int8_t negative_threshold;
    uint8_t destination_core;
    uint8_t destination_axon;
} Neuron;

typedef struct {
    int front, rear, size;
    unsigned capacity;
    uint8_t* array; // destination_axon
} Queue;

typedef struct {
    Neuron neurons[NEURONS_PER_CORE];
    uint8_t synapse_connections[AXONS][GROUPS];
    Queue spikeQueue;
    uint8_t axon_instruction[AXONS/4]; // add
    uint8_t output_axons[NEURONS_PER_CORE];
} SNNCore;

SNNCore cores[NUM_CORES];

Queue* createQueue(unsigned capacity) {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0; 
    queue->rear = capacity - 1;
    queue->array = (uint8_t*)malloc(queue->capacity * sizeof(uint8_t)); // destination_axon
    return queue;
}

int isFull(Queue* queue) { return (queue->size == queue->capacity); }
int isEmpty(Queue* queue) { return (queue->size == 0); }

void push(Queue* queue, uint8_t packet) {
    if (isFull(queue)) return;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->array[queue->rear] = packet;
    queue->size = queue->size + 1;
}

uint8_t pop(Queue* queue) {
    if (isEmpty(queue)) return -1;
    uint8_t packet;
    packet = queue->array[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size = queue->size - 1;
    return packet;
}

int front(Queue* queue) {
    if (isEmpty(queue)) return -1;
    return queue->array[queue->front];
}

void printQueue(Queue* queue) {
    printf("Queue contents: ");
    for (int i = queue->front; i <= queue->rear; i++) 
    {
        printf("%d ", queue->array[i]);
    }
    printf("\n");
}

void initializeCore() {
    for (int i = 0; i < NUM_CORES; i++) 
    {
        cores[i].spikeQueue = *createQueue(FIFO_SIZE);

        for (int j = 0; j < AXONS; j++) 
            for (int k = 0; k < GROUPS; k++) 
                cores[i].synapse_connections[j][k] = 0;
        // add
        for (int j = 0; j < AXONS/4; j++)
            cores[i].axon_instruction[j] = 0;
    }
}

void readNeuronData(SNNCore* core, const char* line, int neuronIndex) {
    for (int i = 0; i < AXONS; i++) 
    {
        int8_t temp = core->synapse_connections[i][neuronIndex/8]; // group_index = neuronIndex/8
        core->synapse_connections[i][neuronIndex/8] = temp | ((line[i] - '0') << (7 - (neuronIndex%8))); // convert to int8_t
        uint8_t temp1 = core->axon_instruction[i/4];
        core->axon_instruction[i/4] = temp1 | ((line[AXONS+2*i] - '0') << (7 - (neuronIndex%8)));
    }

    for (int i = 0, j = AXONS; i < AXONS/4; i++, j += 8) 
    {
        int8_t value = 0;

        for (int k = 0; k < 8; k++) 
            value = (value << 1) | (line[j + k] - '0');

        core->axon_instruction[i] = value;
    }

    int8_t params[11];
    for (int i = 0, j = AXONS + 2*AXONS; i < 11; i++, j += 8) 
    {
        int8_t value = 0;

        for (int k = 0; k < 8; k++) 
            value = (value << 1) | (line[j + k] - '0');

        params[i] = value;
    }

    Neuron* neuron = &core->neurons[neuronIndex];
    neuron->current_membrane_potential = params[0];
    neuron->reset_posi_potential = params[1];
    neuron->reset_nega_potential = neuron->reset_posi_potential;
    
    for (int i = 0; i < 4; i++) 
        neuron->weights[i] = params[2 + i];
    
    neuron->leakage_value = params[6];
    neuron->positive_threshold = params[7];
    neuron->negative_threshold = params[8];
    neuron->destination_axon = params[9];
    neuron->destination_core = params[10];
    printf("Neuron %d: ", neuronIndex);
    for (int i = 0; i <11; i ++)
        printf("%d ", params[i]);
    printf("\n");
}
// Rework
int getNeuronData(SNNCore cores[]) {
    FILE* file = fopen("test_data_modified.txt", "r");
    if (file == NULL) 
    {
        printf("Unable to open file.\n");
        return 1;
    }
    char line[AXONS + 92 + 512];
    int core_index = 0, neuronIndex = 0;
    while (fgets(line, sizeof(line), file)) 
    {
        printf("Core %d - ", core_index);
        readNeuronData(&cores[core_index], line, neuronIndex);
        neuronIndex++;
        if (neuronIndex >= NEURONS_PER_CORE) 
        {
            neuronIndex = 0;
            core_index++;
            if (core_index >= NUM_CORES) break;
        }
    }
    fclose(file);
    return 0;
}
// Rework
Queue* loadSpikesToQueue() {
    Queue* lastSpikeQueue = NULL;
    int count_input = 0;
    char line[260];
    FILE* file = fopen("spike_in.txt", "r");
    FILE* output_file = fopen("input_spike.txt", "w");
    if (output_file == NULL) 
    {
        printf("Can't open input_spike.txt file for writting.\n");
        return NULL;
    }
    for (int core_index = 0; core_index < NUM_CORES; core_index++) 
    {
        int total_spikes = 0;
        fgets(line, sizeof(line), file);
        fprintf(output_file, "Core %d: ", core_index);
        for (int axon_index = 0; axon_index < AXONS; ++axon_index) 
        {
            if (line[axon_index] == '1') {
                total_spikes ++;
                push(&cores[core_index].spikeQueue, axon_index);
                fprintf(output_file, "%d ", axon_index);
            }
        }
        fprintf(output_file, ", Total: %d\n", total_spikes);
        lastSpikeQueue = &cores[core_index].spikeQueue; 
    }
    fclose(output_file); 
    fclose(file);
    return lastSpikeQueue;
}

int neuron_behavior(Neuron* neuron, int axon_instruction){ //axon_instruction
    neuron->current_membrane_potential += neuron->weights[axon_instruction];
    neuron->current_membrane_potential += neuron->leakage_value; // - (previous)

    if (neuron->current_membrane_potential >= neuron->positive_threshold)  // >= (previous)
    {
        neuron->current_membrane_potential = neuron->reset_posi_potential;
        return 1;
    } else if (neuron->current_membrane_potential < neuron->negative_threshold) 
    {
        neuron->current_membrane_potential = neuron->reset_nega_potential;
        return 0;
    }
    return 0;
}

void snn_behavior() {
    FILE* spike_log = fopen("spike_log.txt", "w");
    for (int core_index = 0; core_index < NUM_CORES; core_index ++)
    {    
        printf("\nCore %d - ", core_index);
        printQueue(&cores[core_index].spikeQueue);
        while (!isEmpty(&cores[core_index].spikeQueue)) 
        {
            uint8_t axon_index = pop(&cores[core_index].spikeQueue);
            for (int i = 0; i < NEURONS_PER_CORE; i++) 
            {
                int connected = cores[core_index].synapse_connections[axon_index][i/8] & (1 << (7 - i % 8));
                // check if connection exists
                if (connected) 
                {
                    Neuron* neuron = &cores[core_index].neurons[i];
                    int axon_instruction = (cores[core_index].axon_instruction[i/4] >> (6-i%4)) & 0x03; // extract 2 bits
                    int spike = neuron_behavior(neuron, axon_instruction); //axon_instruction
                    if (spike & (!cores[core_index].output_axons[i]))
                    {
                        cores[core_index].output_axons[i] = 1;
                        if (core_index < (NUM_CORES - 1))
                        {
                            fprintf(spike_log, "Neuron %d of Core %d from axon %d fired spike to axon %d of Core %d\n", i, core_index, axon_index, neuron->destination_axon, neuron->destination_core);
                            push(&cores[neuron->destination_core].spikeQueue, neuron->destination_axon);
                        } else
                        {
                            fprintf(spike_log, "(Destination) Core %d : Neuron %d from axon %d fired spike\n", core_index, i, axon_index);
                        }
                    }
                }
            }
        }
    }
    fclose(spike_log);
}

void saveOutputAxons(SNNCore* cores, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) 
    {
        printf("Unable to open file %s for writing.\n", filename);
        return;
    }
    for (int core_index = 0; core_index < NUM_CORES ; core_index++) 
    {
        fprintf(file, "Core %d\n", core_index);
        for (int axon_index = 0; axon_index < NEURONS_PER_CORE; axon_index++) 
        {
            fprintf(file, "%d", cores[core_index].output_axons[axon_index]);
        }
        if (core_index < NUM_CORES - 1)
            fprintf(file, "\n");
    }
    fclose(file);
}

int main() {
    // Initialize core
    initializeCore();
    // Load parameters into each neuron
    if (getNeuronData(cores)) {
        return 1;
    }
    // Load the first spike_in to queues
    loadSpikesToQueue();
    // SNN behavior
    snn_behavior();
    // Save output axons
    saveOutputAxons(cores, "output_axons.txt");
    printf("Done.\n");
    return 0;
}