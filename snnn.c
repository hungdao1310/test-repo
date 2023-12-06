struct 
{
    int8_t threshold;
    int8_t potential;
    int8_t axon_index, core_index;
} Neuron;

struct 
{
    Neuron neurons[256];
    Packet queue[1000];
    int8_t synapse[256][64];
} Core;

int snn_behavior(Core core[5], Queue queue[5][1000])
{
    for (core_index = 0; core_index < 5; core_index++) {
        while (queue is not empty) {
            get packet from queue;
            core_index = packet.core_index;
            axon_index = packet.axon_index;
            for (int i=0; i<256; i++){
                int gr = i / 8;
                int neuron_idx = i%8;
                connection = core[core_index].synapse[axon_index][gr]; //int8_t
                connected = connection & (1 << neuron_idx);
                if (connected)
                    neuron_behavior(core[core_index].neurons[i], new_packet);
                if (new_packet)
                    push_queue(queue[new_packet.core_index], new_packet);
            }
        }
    }
}

synapse[axon_index][neuron_index/8 & (1 << neuron_index%8)]