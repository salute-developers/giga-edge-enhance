import { SPENCH_WORKLET_NAME } from '../../utils/constants';

import { SpenchBaseWorkletProcessor } from './SpenchBaseWorkletProcessor';

class SpenchWorkletProcessor extends SpenchBaseWorkletProcessor {}

registerProcessor(SPENCH_WORKLET_NAME, SpenchWorkletProcessor);
