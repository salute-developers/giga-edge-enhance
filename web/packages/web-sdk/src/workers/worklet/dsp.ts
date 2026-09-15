import { GDN_DSP_WORKLET_NAME } from '../../utils/constants';

import { GigaDenoiserDspWorkletProcessor } from './GigaDenoiserDspWorkletProcessor';

registerProcessor(GDN_DSP_WORKLET_NAME, GigaDenoiserDspWorkletProcessor);
