import { GEE_DSP_WORKLET_NAME } from '../../utils/constants';

import { GigaEdgeEnhanceDspWorkletProcessor } from './GigaEdgeEnhanceDspWorkletProcessor';

registerProcessor(GEE_DSP_WORKLET_NAME, GigaEdgeEnhanceDspWorkletProcessor);
