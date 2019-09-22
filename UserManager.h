/**************************************************************************
    
    @author   Elmü
    This class manages the users that are stored in the EEPROM of the Teensy.
    
    Check for a new version on:
    http://www.codeproject.com/Articles/1096861/DIY-electronic-RFID-Door-Lock-with-Battery-Backup
  
**************************************************************************/

#ifndef USERMANAGER_H
#define USERMANAGER_H

#include "Utils.h"
#include <EEPROM.h>

// Defines the maximum characters that can be stored for a user name + terminating zero character.
// The smaller this value, the more users fit into the EEPROM.
// The EEPROM is filled with kUser structures of which each one stores the username and 8 byte for the ID and 1 byte for the user flags.
// Normally the EPROM size is a multiple of 32 bytes.
// ATTENTION: When changing this value you must execute the CLEAR command to erase the EEPROM!
#define NAME_BUF_SIZE   (32 - 8 - 1)

enum eUserFlags
{
    NO_DOOR   = 0,
    DOOR_ONE  = 1,
    DOOR_TWO  = 2,
    DOOR_BOTH = DOOR_ONE | DOOR_TWO,
};

// This structure is stored in EEPROM for each user
struct kUser
{
    // Constructor
    kUser()
    {
        memset(this, 0, sizeof(kUser));
    }

    // Card ID (4 or 7 bytes), binary
    union 
    {
        uint64_t  u64;      
        byte      u8[8];
    } ID;
   
    // User name (plain text + terminating zero character) + appended random data if name is shorter than NAME_BUF_SIZE
    char s8_Name[NAME_BUF_SIZE]; 

    // This byte stores eUserFlags (which door(s) to open for this user)
    byte u8_Flags;    
};

class UserManager
{
public:
    static void DeleteAllUsers()
    {
        for (uint16_t i=0; i<EEPROM.length(); i++)
        {
            EEPROM.write(i, 0);
        }
    }
    
    static bool FindUser(uint64_t u64_ID, kUser* pk_User)
    {
        if (u64_ID == 0)
            return false;
      
        for (int U=0; true; U++)
        {
            if (!ReadUserAt(U, pk_User))
                return false;
    
            if (pk_User->ID.u64 == 0)
                return false;
    
            if (pk_User->ID.u64 == u64_ID)
                return true;
        }   
    }
    
    // Insert the user alphabetically sorted into the EEPROM
    static bool StoreNewUser(kUser* pk_NewUser)
    {
        for (int U=0; true; U++)
        {
            kUser k_OldUser;
            if (!ReadUserAt(U, &k_OldUser))
            {
                Utils::Print("Error: The EEPROM is full\r\n");
                return false;
            }
    
            if (k_OldUser.ID.u64 == 0)
            {
                // At position U there is no user -> insert the new user at the end of all current users
                WriteUserAt(U, pk_NewUser); 
                break;
            }
    
            // Insert alphabetically sorted
            if (stricmp(pk_NewUser->s8_Name, k_OldUser.s8_Name) < 0)
            {
                if (!ShiftUsersUp(U)) // Fill position U+1 with the user at position U
                {
                    Utils::Print("Error: The EEPROM is full\r\n");
                    return false;
                }
                WriteUserAt(U, pk_NewUser); 
                break;
            }
        }
        
        Utils::Print("New user stored successfully:\r\n");   
        PrintUser(pk_NewUser);
        return true;
    }
    
    // Deletes a user by ID or by name.
    // To delete by name pass u64_ID = 0.
    // To delete by UID  pass s8_Name = NULL.
    // If the same user name is stored multiple times with different cards, they will all be deleted.
    // To remove only one card of a user give him different names for each card: "John 1", "John 2", "John 3"
    static bool DeleteUser(uint64_t u64_ID, char* s8_Name)
    {
        bool b_Success = false;
        for (int U=0; true; U++)
        {
            kUser k_User;
            if (!ReadUserAt(U, &k_User))
                return b_Success;
    
            if ((u64_ID  != 0    && u64_ID == k_User.ID.u64) ||
                (s8_Name != NULL && stricmp(s8_Name, k_User.s8_Name) == 0))
            {
                ShiftUsersDown(U); // Fill position U with the user at position U+1, etc..
                U --;              // Now there is another user at position U -> check U again
                b_Success = true;

                char s8_Buf[100];
                sprintf(s8_Buf, "The user '%s' has been deleted.\r\n", k_User.s8_Name);
                Utils::Print(s8_Buf);
            }
        }
    }

    // Modifies the flags of a user.
    // returns false if the user does not exist.
    static bool SetUserFlags(char* s8_Name, byte u8_NewFlags)
    {
        bool b_Success = false;
        for (int U=0; true; U++)
        {
            kUser k_User;
            if (!ReadUserAt(U, &k_User))
                return b_Success;
    
            if (stricmp(s8_Name, k_User.s8_Name) == 0)
            {
                k_User.u8_Flags = u8_NewFlags;
                WriteUserAt(U, &k_User);
                PrintUser(&k_User);
                b_Success = true;
            }
        }
    }
          
    // Prints lines like 
    // "Claudia             6D 2F 8A 44 00 00 00    (door 1)"   
    // "Johnathan           10 FC D9 33 00 00 00    (door 1 + 2)"   
    static void PrintUser(kUser* pk_User)
    {
        Utils::Print(pk_User->s8_Name);
    
        int s32_Spaces = NAME_BUF_SIZE - strlen(pk_User->s8_Name) + 2;
        for (int i=0; i<s32_Spaces; i++)
        {
            Utils::Print(" ");
        }
    
        // The ID may be 4 or 7 bytes long
        Utils::PrintHexBuf(pk_User->ID.u8, 7);

        switch (pk_User->u8_Flags & DOOR_BOTH)
        {
            case DOOR_ONE:  Utils::Print("   (door 1)\r\n");            break;
            case DOOR_TWO:  Utils::Print("   (door 2)\r\n");            break;
            case DOOR_BOTH: Utils::Print("   (door 1 + 2)\r\n");        break;
            default:        Utils::Print("   (no door specified)\r\n"); break;
        }
    }

    static void ListAllUsers()
    {
        Utils::Print("Users stored in EEPROM:\r\n");

        kUser k_User;        
        for (int U=0; true; U++)
        {
            if (!ReadUserAt(U, &k_User))
                break;

            if (k_User.ID.u64 == 0)
            {
                if (U == 0) Utils::Print("No users.\r\n");
                break;
            }
                
            PrintUser(&k_User);
        }
    }

private:
    // Writes one user to the EEPROM
    // returns false if index out of range
    static bool WriteUserAt(int s32_Index, kUser* pk_User)
    {
        uint32_t P = s32_Index * sizeof(kUser);
        if (P + sizeof(kUser) > EEPROM.length())
            return false;
    
        byte* pu8_Ptr = (byte*)pk_User;
        for (uint32_t i=0; i<sizeof(kUser); i++)
        {
            EEPROM.write(P + i, pu8_Ptr[i]);
        }
        return true;
    }
    
    // Reads one user from the EEPROM
    // returns false if index out of range
    static bool ReadUserAt(int s32_Index, kUser* pk_User)
    {
        uint32_t P = s32_Index * sizeof(kUser);
        if (P + sizeof(kUser) > EEPROM.length())
            return false;
    
        byte* pu8_Ptr = (byte*)pk_User;
        for (uint32_t i=0; i<sizeof(kUser); i++)
        {
            pu8_Ptr[i] = EEPROM.read(P + i);
        }
        return true;
    }

    // A new user will be inserted at position U --> make space by shifting all the following users up
    // Required to maintain alphabetical order.
    // returns false if the EEPROM is full
    static bool ShiftUsersUp(int U)
    {
        kUser k_User;
        int s32_Last = (EEPROM.length() / sizeof(kUser)) -1;
        if (!ReadUserAt(s32_Last, &k_User))
            return false; // this should never happen!
    
        if (k_User.ID.u64 != 0)
            return false; // The EEPROM is full
    
        for (int P=s32_Last; P>U; P--)
        {
            ReadUserAt (P-1, &k_User);
            WriteUserAt(P,   &k_User);
        }
        return true;
    }
    
    // The user at position U is deleted -> shift down all the following users
    // Required to maintain alphabetical order.
    static void ShiftUsersDown(int U)
    {
        kUser k_User;
        for (int P=U; true; P++)
        {
            if (!ReadUserAt(P+1, &k_User))
            {
                // If there is no user that can be shifted down, delete the user at position P
                k_User.ID.u64 = 0;
            }
            
            WriteUserAt(P, &k_User);
    
            if (k_User.ID.u64 == 0)
                break;
        }
    }
};

#endif // USERMANAGER_H
