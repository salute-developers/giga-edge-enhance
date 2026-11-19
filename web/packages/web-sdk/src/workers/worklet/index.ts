import { GEE_WORKLET_NAME } from '../../utils/constants';

import { GigaEdgeEnhanceBaseWorkletProcessor } from './GigaEdgeEnhanceBaseWorkletProcessor';

class GigaEdgeEnhanceWorkletProcessor extends GigaEdgeEnhanceBaseWorkletProcessor {}

registerProcessor(GEE_WORKLET_NAME, GigaEdgeEnhanceWorkletProcessor);
