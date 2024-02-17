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
        uint8_t temp = core->synapse_connections[i][neuronIndex/8]; // group_index = neuronIndex/8
        core->synapse_connections[i][neuronIndex/8] = temp | ((line[i] - '0') << (7 - (neuronIndex%8))); // convert to int8_t
    }

    int8_t params1[9];
    for (int i = 0, j = AXONS; i < 9; i++, j += 8) 
    {
        int8_t value = 0;

        for (int k = 0; k < 8; k++) 
            value = (value << 1) | (line[j + k] - '0');

        params1[i] = value;
    }

    uint8_t params2[2];
    for (int i = 0, j = AXONS + 8*9; i < 2; i++, j += 8) 
    {
        uint8_t value = 0;

        for (int k = 0; k < 8; k++) 
            value = (value << 1) | (line[j + k] - '0');

        params2[i] = value;
    }

    Neuron* neuron = &core->neurons[neuronIndex];
    neuron->current_membrane_potential = params1[0];
    neuron->reset_posi_potential = params1[1];
    neuron->reset_nega_potential = neuron->reset_posi_potential;
    
    for (int i = 0; i < 4; i++) 
        neuron->weights[i] = params1[2 + i];
    
    neuron->leakage_value = params1[6];
    neuron->positive_threshold = params1[7];
    neuron->negative_threshold = params1[8];
    neuron->destination_axon = params2[0];
    neuron->destination_core = params2[1];
    printf("Neuron %d: ", neuronIndex);
    for (int i = 0; i < 9; i ++)
        printf("%d ", params1[i]);
    for (int i = 0; i < 2; i ++)
        printf("%d ", params2[i]);
    printf("\n");
}

// Load axon_instruction
void readAxonData(SNNCore *core, const char *axon_line)
{
    //printf("Axon instruction: ");
    for (int i = 0, j = 0; i < AXONS/4; i++, j += 8) 
    {
        uint8_t value = 0;

        for (int k = 0; k < 8; k++) 
            value = (value << 1) | (axon_line[j + k] - '0');

        core->axon_instruction[i] = value;
        //printf("%d ", core->axon_instruction[i]);
    }
    //printf("\n");
}

int getNeuronData(SNNCore cores[]) {
    FILE* file = fopen("neuron_data.txt", "r");
    FILE* file_axon = fopen("axon_instruction.txt", "r");
    if (file == NULL || file_axon == NULL) 
    {
        printf("Unable to open file.\n");
        return 1;
    }
    char line[AXONS + 92];
    char axon_line[2*AXONS+4];
    int core_index = 0, neuronIndex = 0;
    while (fgets(line, sizeof(line), file)) 
    {
        printf("Core %d - ", core_index);
        readNeuronData(&cores[core_index], line, neuronIndex);
        neuronIndex++;
        if (neuronIndex >= NEURONS_PER_CORE) 
        {
            neuronIndex = 0;
            fgets(axon_line, sizeof(axon_line), file_axon);
            readAxonData(&cores[core_index], axon_line);
            core_index++;
            if (core_index >= NUM_CORES) break;
        }
    }
    fclose(file_axon);
    fclose(file);
    return 0;
}

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

int neuron_behavior(Neuron* neuron, int is_connected, int axon_instruction, int is_last_spike)
{
    // Calculate current membrane potetial of the neuron
    neuron->current_membrane_potential += neuron->weights[axon_instruction] * is_connected;
    // Determine if is this the last spike event of a neuron
    if (is_last_spike)
    {
        neuron->current_membrane_potential += neuron->leakage_value;
        if (neuron->current_membrane_potential >= neuron->positive_threshold) 
        {
            neuron->current_membrane_potential = neuron->reset_posi_potential;
            return 1;
        } else if (neuron->current_membrane_potential < neuron->negative_threshold) 
        {
            neuron->current_membrane_potential = neuron->reset_nega_potential;
            return 0;
        }
    }
    else
    {
        neuron->current_membrane_potential = neuron->current_membrane_potential;
        return 0;
    }
}

void snn_behavior() {
    FILE* spike_log = fopen("spike_log.txt", "w");
    for (int core_index = 0; core_index < NUM_CORES; core_index ++)
    {    
        printf("\nCore %d - ", core_index);
        printQueue(&cores[core_index].spikeQueue);
        int size = cores[core_index].spikeQueue.size;
        for (int i = 0; i < NEURONS_PER_CORE; i++) 
        {
            for (int j = 1; j <= size; j++) 
            {
                // Get spike packet (destination axon index) from Core's queue
                uint8_t axon_index = pop(&cores[core_index].spikeQueue);
                Neuron* neuron = &cores[core_index].neurons[i];
                int axon_instruction = (cores[core_index].axon_instruction[i/4] >> (6-i%4)) & 0x03; // extract 2 bits
                // Determine if there is a connetion between axon and neuron
                int is_connected = cores[core_index].synapse_connections[axon_index][i/8] & (1 << (7 - i % 8));
                // Determine if is this the last spike event of a neuron
                int is_last_spike = (j == size)?1:0;
                // Implement neuron behavior
                int is_spike = neuron_behavior(neuron, is_connected, axon_instruction, is_last_spike); //axon_instruction
                cores[core_index].neurons[i] = *neuron;
                // Check if a neuron fired spike
                if (is_spike)
                {
                    if (core_index < (NUM_CORES - 1))
                    {
                        fprintf(spike_log, "Neuron %d of Core %d fired spike to axon %d of Core %d\n", i, core_index, neuron->destination_axon, neuron->destination_core);
                        if (!cores[core_index].output_axons[i])
                            push(&cores[neuron->destination_core].spikeQueue, neuron->destination_axon);
                    } else // Output Core
                    {
                        fprintf(spike_log, "(Output) Core %d : Neuron %d fired spike\n", core_index, i);
                    }
                    cores[core_index].output_axons[i] = 1;
                }
                push(&cores[core_index].spikeQueue, axon_index);
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