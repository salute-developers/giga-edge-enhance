import { handleControllerMessage, global } from './GigaEdgeSpenchWorker';

global.addEventListener('message', handleControllerMessage);
