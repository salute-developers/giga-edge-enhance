import { SPENCH_WORKLET_NAME } from '../../utils/constants';

import { SpenchDspWorkletProcessor } from './SpenchDspWorkletProcessor';

registerProcessor(SPENCH_WORKLET_NAME, SpenchDspWorkletProcessor);
