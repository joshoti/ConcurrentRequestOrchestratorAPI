#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

/**
 * @brief Registers the websocket handler with the log router.
 * 
 * This function should be called during initialization to register
 * all websocket publishing functions with the log routing system.
 */
void websocket_handler_register(void);

#endif // WEBSOCKET_HANDLER_H
