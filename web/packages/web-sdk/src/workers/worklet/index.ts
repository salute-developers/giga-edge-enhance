import { GDN_WORKLET_NAME } from '../../utils/constants';

import { GigaDenoiserBaseWorkletProcessor } from './GigaDenoiserBaseWorkletProcessor';

class GigaDenoiserWorkletProcessor extends GigaDenoiserBaseWorkletProcessor {}

registerProcessor(GDN_WORKLET_NAME, GigaDenoiserWorkletProcessor);
