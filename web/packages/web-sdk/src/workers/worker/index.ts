import { handleControllerMessage, global } from './GigaEdgeEnhanceWorker';

global.addEventListener('message', handleControllerMessage);
