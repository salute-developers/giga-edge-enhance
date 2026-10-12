import { GES_WORKLET_NAME } from '../../utils/constants';

import { GigaEdgeSpenchBaseWorkletProcessor } from './GigaEdgeSpenchBaseWorkletProcessor';

class GigaEdgeSpenchWorkletProcessor extends GigaEdgeSpenchBaseWorkletProcessor {}

registerProcessor(GES_WORKLET_NAME, GigaEdgeSpenchWorkletProcessor);
